// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
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

#include <inttypes.h>
#include <limits.h>
#include <string.h>
#include <hpx/hpx.h>
#include <libhpx/boot.h>
#include <libhpx/debug.h>
#include <libhpx/gas.h>
#include <libhpx/libhpx.h>
#include <libhpx/memory.h>
#include <libhpx/locality.h>
#include <libhpx/network.h>
#include "bitmap.h"
#include "gpa.h"
#include "heap.h"
#include "pgas.h"
#include "../parcel/emulation.h"

/// The PGAS type is a global address space that manages a shared heap.
///
/// @note This is sort of silly. The PGAS gas is basically an instance of an
///       object that uses the heap. A more normal approach to this would be to
///       make the heap an instance variable of the PGAS gas subtype. The reason
///       we're currently doing the global_heap as a static is that other files
///       in the pgas module want to interact with the global_heap directly, and
///       we didn't really want to expose the entire PGAS class. For a nicer
///       look, we could adjust this and make everyone in PGAS that needs the
///       heap go through a cast of the locality gas reference, e.g.,
///       (pgas_gas_t*)here->gas->heap.
///
heap_t *global_heap = NULL;

static void _pgas_delete(gas_t *gas) {
  if (global_heap) {
    heap_fini(global_heap);
    free(global_heap);
    global_heap = NULL;
  }
}

static bool _pgas_is_global(gas_t *gas, void *lva) {
  return heap_contains_lva(global_heap, lva);
}

static bool _gpa_is_cyclic(hpx_addr_t gpa) {
  return heap_offset_is_cyclic(global_heap, pgas_gpa_to_offset(gpa));
}

hpx_addr_t pgas_lva_to_gpa(void *lva) {
  const uint64_t offset = heap_lva_to_offset(global_heap, lva);
  return pgas_offset_to_gpa(here->rank, offset);
}

void *pgas_gpa_to_lva(hpx_addr_t gpa) {
   const uint64_t offset = pgas_gpa_to_offset(gpa);
   return heap_offset_to_lva(global_heap, offset);
}

void *pgas_offset_to_lva(uint64_t offset) {
  return heap_offset_to_lva(global_heap, offset);
}

uint64_t pgas_max_offset(void) {
  return (1ull << GPA_OFFSET_BITS);
}

static int64_t _pgas_sub(hpx_addr_t lhs, hpx_addr_t rhs, uint32_t bsize) {
  const bool l = _gpa_is_cyclic(lhs);
  const bool r = _gpa_is_cyclic(rhs);
  dbg_assert_str(l == r, "cannot compare addresses across allocations.\n");
  dbg_assert_str(!(l ^ r), "cannot compare cyclic with non-cyclic.\n");

  return (l && r) ? pgas_gpa_sub_cyclic(lhs, rhs, bsize)
                  : pgas_gpa_sub(lhs, rhs);
}

static hpx_addr_t _pgas_add(hpx_addr_t gpa, int64_t bytes, uint32_t bsize) {
  const bool cyclic = _gpa_is_cyclic(gpa);
  return (cyclic) ? pgas_gpa_add_cyclic(gpa, bytes, bsize)
                  : pgas_gpa_add(gpa, bytes);
}


// Compute a global address for a locality.
static hpx_addr_t _pgas_there(uint32_t i) {
  hpx_addr_t there = pgas_offset_to_gpa(i, UINT64_MAX);
  if (DEBUG) {
    uint64_t offset = pgas_gpa_to_offset(there);
    dbg_assert_str(!heap_contains_offset(global_heap, offset),
                   "HPX_THERE() out of expected range\n");
  }
  return there;
}


/// Pin and translate an hpx address into a local virtual address. PGAS
/// addresses don't get pinned, so we're really only talking about translating
/// the address if its local.
static bool _pgas_try_pin(const hpx_addr_t gpa, void **local) {
  dbg_assert_str(gpa, "cannot pin HPX_NULL\n");

  // we're safe for HPX_HERE/THERE because gpa_to_rank doesn't range-check
  if (pgas_gpa_to_rank(gpa) != here->rank) {
    return false;
  }

  // special case messages to "here"
  if (local) {
    *local = (gpa != HPX_HERE) ? pgas_gpa_to_lva(gpa) : &here;
  }

  return true;
}

static void _pgas_unpin(const hpx_addr_t addr) {
  dbg_assert_str(_pgas_try_pin(addr, NULL), "%"PRIu64" is not local to %u\n",
                 addr, here->rank);
}


static hpx_addr_t _pgas_gas_cyclic_alloc(size_t n, uint32_t bsize) {
  hpx_addr_t addr;
  if (here->rank == 0) {
    addr = pgas_cyclic_alloc_sync(n, bsize);
  }
  else {
    int e = hpx_call_sync(HPX_THERE(0), pgas_cyclic_alloc, &addr, sizeof(addr),
                          &n, &bsize);
    dbg_check(e, "Failed to call pgas_cyclic_alloc_handler.\n");
  }
  dbg_assert_str(addr != HPX_NULL, "HPX_NULL is not a valid allocation\n");
  return addr;
}

static hpx_addr_t _pgas_gas_cyclic_calloc(size_t n, uint32_t bsize) {
  hpx_addr_t addr;
  if (here->rank == 0) {
    addr = pgas_cyclic_calloc_sync(n, bsize);
  }
  else {
    int e = hpx_call_sync(HPX_THERE(0), pgas_cyclic_calloc, &addr, sizeof(addr),
                          &n, &bsize);
    dbg_check(e, "Failed to call pgas_cyclic_calloc_handler.\n");
  }
  dbg_assert_str(addr != HPX_NULL, "HPX_NULL is not a valid allocation\n");
  return addr;
}

/// Allocate a single global block from the global heap, and return it as an
/// hpx_addr_t.
static hpx_addr_t _pgas_gas_alloc(uint32_t bytes) {
  void *lva = global_malloc(bytes);
  dbg_assert(heap_contains_lva(global_heap, lva));
  return pgas_lva_to_gpa(lva);
}

/// Free a global address.
///
/// This global address must either be the base of a cyclic allocation, or a
/// block allocated by _pgas_gas_alloc. At this time, we do not attempt to deal
/// with the cyclic allocations, as they are using a simple csbrk allocator.
static void _pgas_gas_free(hpx_addr_t gpa, hpx_addr_t sync) {
  if (gpa == HPX_NULL) {
    return;
  }

  const uint64_t offset = pgas_gpa_to_offset(gpa);

  const void *lva = heap_offset_to_lva(global_heap, offset);
  dbg_assert_str(heap_contains_lva(global_heap, lva),
                 "attempt to free out of bounds offset %"PRIu64"", offset);

  if (heap_offset_is_cyclic(global_heap, offset)) {
    heap_free_cyclic(global_heap, offset);
  }
  else if (pgas_gpa_to_rank(gpa) == here->rank) {
    global_free(pgas_gpa_to_lva(offset));
  }
  else {
    int e = hpx_call(gpa, pgas_free, sync, NULL, 0);
    dbg_check(e, "failed to call pgas_free on %"PRIu64"", gpa);
    return;
  }

  hpx_lco_set(sync, 0, NULL, HPX_NULL, HPX_NULL);
}

static int _pgas_parcel_memcpy(hpx_addr_t to, hpx_addr_t from, size_t size,
                               hpx_addr_t sync) {
  if (!size) {
    return HPX_SUCCESS;
  }

  const uint32_t rank = here->rank;
  if (pgas_gpa_to_rank(to) == rank && pgas_gpa_to_rank(from) == rank) {
    void *lto = pgas_gpa_to_lva(to);
    const void *lfrom = pgas_gpa_to_lva(from);
    memcpy(lto, lfrom, size);
  }
  else {
    return parcel_memcpy(to, from, size, sync);
  }

  hpx_lco_set(sync, 0, NULL, HPX_NULL, HPX_NULL);
  return HPX_SUCCESS;
}

static int _pgas_memput(hpx_addr_t to, const void *from, size_t n,
                        hpx_addr_t lsync, hpx_addr_t rsync) {
  if (!n) {
    return HPX_SUCCESS;
  }

  if (pgas_gpa_to_rank(to) == here->rank) {
    void *lto = pgas_gpa_to_lva(to);
    memcpy(lto, from, n);
    hpx_lco_set(lsync, 0, NULL, HPX_NULL, HPX_NULL);
    hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
    return HPX_SUCCESS;
  }
  else if (rsync) {
    return network_pwc(here->network, to, from, n, lco_set, lsync, memput_rsync,
                       rsync);
  }
  else {
    return network_put(here->network, to, from, n, lco_set, lsync);
  }
}

static int _pgas_memget(void *to, hpx_addr_t from, size_t n, hpx_addr_t lsync) {
  if (!n) {
    return HPX_SUCCESS;
  }

  if (pgas_gpa_to_rank(from) == here->rank) {
    const void *lfrom = pgas_gpa_to_lva(from);
    memcpy(to, lfrom, n);
    hpx_lco_set(lsync, 0, NULL, HPX_NULL, HPX_NULL);
    return HPX_SUCCESS;
  }
  else {
    return network_get(here->network, to, from, n, lco_set, lsync);
  }
}

static void _pgas_move(hpx_addr_t src, hpx_addr_t dst, hpx_addr_t sync) {
  hpx_lco_set(sync, 0, NULL, HPX_NULL, HPX_NULL);
}

static size_t _pgas_local_size(gas_t *gas) {
  return global_heap->nbytes;
}

static void *_pgas_local_base(gas_t *gas) {
  return global_heap->base;
}

static uint64_t _pgas_offset_of(hpx_addr_t gpa) {
  return pgas_gpa_to_offset(gpa);
}

static void *_pgas_mmap(void *gas, void *addr, size_t n, size_t align) {
  dbg_assert(global_heap);
  return heap_chunk_alloc(global_heap, addr, n, align);
}

static void _pgas_munmap(void *gas, void *addr, size_t n) {
  dbg_assert(global_heap);
  heap_chunk_dalloc(global_heap, addr, n);
}

static gas_t _pgas_vtable = {
  .type          = HPX_GAS_PGAS,
  .delete        = _pgas_delete,
  .is_global     = _pgas_is_global,
  .local_size    = _pgas_local_size,
  .local_base    = _pgas_local_base,
  .locality_of   = pgas_gpa_to_rank,
  .sub           = _pgas_sub,
  .add           = _pgas_add,
  .lva_to_gva    = pgas_lva_to_gpa,
  .gva_to_lva    = pgas_gpa_to_lva,
  .there         = _pgas_there,
  .try_pin       = _pgas_try_pin,
  .unpin         = _pgas_unpin,
  .cyclic_alloc  = _pgas_gas_cyclic_alloc,
  .cyclic_calloc = _pgas_gas_cyclic_calloc,
  .local_alloc   = _pgas_gas_alloc,
  .free          = _pgas_gas_free,
  .move          = _pgas_move,
  .memget        = _pgas_memget,
  .memput        = _pgas_memput,
  .memcpy        = _pgas_parcel_memcpy,
  .owner_of      = pgas_gpa_to_rank,
  .offset_of     = _pgas_offset_of,
  .mmap          = _pgas_mmap,
  .munmap        = _pgas_munmap
};

gas_t *gas_pgas_new(const config_t *cfg, boot_t *boot)
{
  size_t heap_size = cfg->heapsize;

  if (here->ranks == 1) {
    log_gas("PGAS requires at least two ranks\n");
    return NULL;
  }

  if (global_heap) {
    return &_pgas_vtable;
  }

  global_heap = malloc(sizeof(*global_heap));
  if (!global_heap) {
    dbg_error("could not allocate global heap\n");
    goto unwind0;
  }

  int e = heap_init(global_heap, heap_size, (here->rank == 0));
  if (e != LIBHPX_OK) {
    dbg_error("failed to allocate global heap\n");
    goto unwind1;
  }

  return &_pgas_vtable;

 unwind1:
  free(global_heap);
 unwind0:
  return NULL;
}
