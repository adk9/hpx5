// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <limits.h>
#include <string.h>
#include <hpx/hpx.h>
#include "libhpx/boot.h"
#include "libhpx/debug.h"
#include "libhpx/gas.h"
#include "libhpx/libhpx.h"
#include "libhpx/locality.h"
#include "../bitmap.h"
#include "../mallctl.h"
#include "../malloc.h"
#include "gva.h"
#include "heap.h"
#include "pgas.h"
#include "../parcel/emulation.h"

/// The PGAS type is a global address space that manages a shared heap.
heap_t *global_heap = NULL;

static void _pgas_delete(gas_class_t *gas) {
  if (global_heap) {
    heap_fini(global_heap);
    free(global_heap);
    global_heap = NULL;
  }
}

static bool _pgas_is_global(gas_class_t *gas, void *addr) {
  return heap_contains(global_heap, addr);
}

static uint32_t _check_cyclic(hpx_addr_t gva, uint32_t bsize) {
  bool cyclic = heap_offset_is_cyclic(global_heap, (uint64_t)gva);

  if (cyclic && !bsize) {
    dbg_error("heap offset %lu is cyclic but user defined block size 0\n", gva);
  }

  if (!cyclic && bsize) {
    dbg_log_gas("heap offset %lu specific bsize %u suppressed\n", gva, bsize);
    return 0;
  }

  return pgas_fit_log2_32(bsize);
}

static int64_t _pgas_sub(hpx_addr_t lhs, hpx_addr_t rhs, uint32_t bsize) {
  const uint32_t lbs = _check_cyclic(lhs, bsize);
  const uint32_t rbs = _check_cyclic(rhs, bsize);
  DEBUG_IF (lbs != rbs) {
    dbg_error("cannot compare addresses between different allocations.\n");
  }
  return pgas_gva_sub(lhs, rhs, here->ranks, lbs);
}

static hpx_addr_t _pgas_add(hpx_addr_t gva, int64_t bytes, uint32_t bsize) {
  hpx_addr_t gva1 = HPX_NULL;
  if (!_check_cyclic(gva, bsize))
    gva1 = pgas_gva_add(gva, bytes, here->ranks);
  else
    gva1 = pgas_gva_add_cyclic(gva, bytes, here->ranks, bsize);
  return gva1;
}


/// Convert a local virtual address into a globa address.
static hpx_addr_t _pgas_lva_to_gva(void *lva) {
  DEBUG_IF (!heap_contains(global_heap, lva)) {
    dbg_error("the global heap does not contain %p", lva);
  }

  const uint64_t heap_offset = heap_offset_of(global_heap, lva);
  const uint32_t rank = here->rank;
  const uint32_t ranks = here->ranks;
  return pgas_gva_from_heap_offset(rank, heap_offset, ranks);
}


// Compute a global address for a locality.
hpx_addr_t _pgas_there(uint32_t i) {
  const uint32_t ranks = here->ranks;
  return pgas_gva_from_heap_offset(i, 0, ranks);
}


/// Pin and translate an hpx address into a local virtual address. PGAS
/// addresses don't get pinned, so we're really only talking about translating
/// the address if its local.
bool pgas_try_pin(const hpx_addr_t gva, void **local) {
  const uint32_t ranks = here->ranks;
  const uint32_t locality = pgas_gva_to_rank(gva);

  if (locality != here->rank) {
    return false;
  }

  if (local) {
    const uint64_t heap_offset = pgas_gva_heap_offset_of(gva, ranks);
    *local = heap_offset_to_local(global_heap, heap_offset);
  }

  return true;
}

static void *_pgas_gva_to_lva(hpx_addr_t addr) {
  void *local = NULL;
  bool is_local = pgas_try_pin(addr, &local);
  DEBUG_IF (!is_local) {
    dbg_error("%lu is not local to %u\n", addr, here->rank);
  }
  return local;
}

static void _pgas_unpin(const hpx_addr_t addr) {
  DEBUG_IF(!pgas_try_pin(addr, NULL)) {
    dbg_error("%lu is not local to %u\n", addr, here->rank);
  }
}

static hpx_addr_t _pgas_gas_cyclic_alloc(size_t n, uint32_t bsize) {
  if (here->rank == 0)
    return pgas_cyclic_alloc_sync(n, bsize);

  hpx_addr_t addr;
  pgas_alloc_args_t args = {
    .n = n,
    .bsize = bsize
  };
  int e = hpx_call_sync(HPX_THERE(0), pgas_cyclic_alloc, &args, sizeof(args),
                        &addr, sizeof(addr));
  dbg_check(e, "Failed to call pgas_cyclic_alloc_handler.\n");
  return addr;
}

static hpx_addr_t _pgas_gas_cyclic_calloc(size_t n, uint32_t bsize) {
  if (here->rank == 0)
    return pgas_cyclic_calloc_sync(n, bsize);

  hpx_addr_t addr;
  pgas_alloc_args_t args = {
    .n = n,
    .bsize = bsize
  };
  int e = hpx_call_sync(HPX_THERE(0), pgas_cyclic_calloc,
                        &args, sizeof(args), &addr, sizeof(addr));
  dbg_check(e, "Failed to call pgas_cyclic_calloc_handler.\n");
  return addr;
}

/// Allocate a single global block from the global heap, and return it as an
/// hpx_addr_t.
static hpx_addr_t _pgas_gas_alloc(uint32_t bytes) {
  void *lva = pgas_global_malloc(bytes);
  assert(lva && heap_contains(global_heap, lva));

  const uint64_t heap_offset = heap_offset_of(global_heap, lva);
  const uint32_t rank = here->rank;
  const uint32_t ranks = here->ranks;
  return pgas_gva_from_heap_offset(rank, heap_offset, ranks);
}

/// Free a global address.
///
/// This global address must either be the base of a cyclic allocation, or a
/// block allocated by _pgas_gas_alloc. At this time, we do not attempt to deal
/// with the cyclic allocations, as they are using a simple csbrk allocator.
static void _pgas_gas_free(hpx_addr_t gva, hpx_addr_t sync) {
  const uint32_t ranks = here->ranks;
  const uint64_t heap_offset = pgas_gva_heap_offset_of(gva, ranks);

  DEBUG_IF (!heap_offset_inbounds(global_heap, heap_offset)) {
    dbg_error("attempt to free out of bounds offset %lu", heap_offset);
  }

  if (heap_offset_is_cyclic(global_heap, heap_offset)) {
    dbg_log_gas("global free of cyclic address detected, HPX does not currently "
                "handle this operation");
  }
  else if (here->rank == pgas_gva_to_rank(gva)) {
    pgas_global_free(heap_offset_to_local(global_heap, heap_offset));
  }
  else {
    int e = hpx_call(gva, pgas_free, NULL, 0, sync);
    dbg_check(e, "failed to call pgas_free on %lu", gva);
    return;
  }

  if (sync != HPX_NULL)
    hpx_lco_set(sync, 0, NULL, HPX_NULL, HPX_NULL);
}

static int _pgas_parcel_memcpy(hpx_addr_t to, hpx_addr_t from, size_t size,
                               hpx_addr_t sync) {
  if (!size)
    return HPX_SUCCESS;

  const uint32_t rank = here->rank;
  if (pgas_gva_to_rank(to) == rank && pgas_gva_to_rank(from) == rank) {
    void *lto = gva_to_lva(to);
    const void *lfrom = gva_to_lva(from);
    memcpy(lto, lfrom, size);
  }
  else {
    return parcel_memcpy(to, from, size, sync);
  }

  if (sync)
    hpx_lco_set(sync, 0, NULL, HPX_NULL, HPX_NULL);

  return HPX_SUCCESS;
}

static int _pgas_parcel_memput(hpx_addr_t to, const void *from, size_t size,
                               hpx_addr_t lsync, hpx_addr_t rsync) {
  if (!size)
    return HPX_SUCCESS;

  if (pgas_gva_to_rank(to) == here->rank) {
    void *lto = gva_to_lva(to);
    memcpy(lto, from, size);
  }
  else {
    return parcel_memput(to, from, size, lsync, rsync);
  }

  if (lsync)
    hpx_lco_set(lsync, 0, NULL, HPX_NULL, HPX_NULL);
  if (rsync)
    hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
  return HPX_SUCCESS;
}

static int _pgas_parcel_memget(void *to, hpx_addr_t from, size_t size,
                               hpx_addr_t lsync) {
  if (!size)
    return HPX_SUCCESS;

  if (pgas_gva_to_rank(from) == here->rank) {
    const void *lfrom = gva_to_lva(from);
    memcpy(to, lfrom, size);
  }
  else {
    return parcel_memget(to, from, size, lsync);
  }

  if (lsync)
    hpx_lco_set(lsync, 0, NULL, HPX_NULL, HPX_NULL);
  return HPX_SUCCESS;
}

static void _pgas_move(hpx_addr_t src, hpx_addr_t dst, hpx_addr_t sync) {
  if (sync)
    hpx_lco_set(sync, 0, NULL, HPX_NULL, HPX_NULL);
}

static gas_class_t _pgas_vtable = {
  .type   = HPX_GAS_PGAS,
  .delete = _pgas_delete,
  .join   = pgas_join,
  .leave  = pgas_leave,
  .is_global = _pgas_is_global,
  .global = {
    .malloc         = pgas_global_malloc,
    .free           = pgas_global_free,
    .calloc         = pgas_global_calloc,
    .realloc        = pgas_global_realloc,
    .valloc         = pgas_global_valloc,
    .memalign       = pgas_global_memalign,
    .posix_memalign = pgas_global_posix_memalign
  },
  .local  = {
    .malloc         = pgas_local_malloc,
    .free           = pgas_local_free,
    .calloc         = pgas_local_calloc,
    .realloc        = pgas_local_realloc,
    .valloc         = pgas_local_valloc,
    .memalign       = pgas_local_memalign,
    .posix_memalign = pgas_local_posix_memalign
  },
  .locality_of   = pgas_gva_to_rank,
  .sub           = _pgas_sub,
  .add           = _pgas_add,
  .lva_to_gva    = _pgas_lva_to_gva,
  .gva_to_lva    = _pgas_gva_to_lva,
  .there         = _pgas_there,
  .try_pin       = pgas_try_pin,
  .unpin         = _pgas_unpin,
  .cyclic_alloc  = _pgas_gas_cyclic_alloc,
  .cyclic_calloc = _pgas_gas_cyclic_calloc,
  .local_alloc   = _pgas_gas_alloc,
  .free          = _pgas_gas_free,
  .move          = _pgas_move,
  .memget        = _pgas_parcel_memget,
  .memput        = _pgas_parcel_memput,
  .memcpy        = _pgas_parcel_memcpy,
  .owner_of      = pgas_gva_to_rank
};

gas_class_t *gas_pgas_new(size_t heap_size, boot_class_t *boot,
                          struct transport_class *transport) {
  if (here->ranks == 1) {
    dbg_log_gas("PGAS requires at least two ranks\n");
    return NULL;
  }

  if (global_heap)
    return &_pgas_vtable;

  global_heap = malloc(sizeof(*global_heap));
  if (!global_heap) {
    dbg_error("pgas: could not allocate global heap\n");
    return NULL;
  }

  int e = heap_init(global_heap, heap_size);
  if (e) {
    dbg_error("pgas: failed to allocate global heap\n");
    free(global_heap);
    return NULL;
  }

  if (heap_bind_transport(global_heap, transport)) {
    if (!mallctl_disable_dirty_page_purge()) {
      dbg_error("pgas: failed to disable dirty page purging\n");
      return NULL;
    }
  }

  return &_pgas_vtable;
}
