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

static uint32_t _pgas_locality_of(hpx_addr_t gva) {
  return pgas_gva_locality_of(gva.offset, here->ranks);
}

static bool _check_cyclic(hpx_addr_t gva, uint32_t *bsize) {
  bool cyclic = heap_offset_is_cyclic(global_heap, gva.offset);
  if (cyclic)
    *bsize = 1;
  return cyclic;
}

static uint64_t _pgas_offset_of(hpx_addr_t gva, uint32_t bsize) {
  DEBUG_IF (!bsize && heap_offset_is_cyclic(global_heap, gva.offset)) {
    dbg_error("invalid block size for cyclic address\n");
  }

  _check_cyclic(gva, &bsize);
  return pgas_gva_offset_of(gva.offset, here->ranks, bsize);
}

static uint32_t _pgas_phase_of(hpx_addr_t gva, uint32_t bsize) {
  _check_cyclic(gva, &bsize);
  return pgas_gva_phase_of(gva.offset, bsize);
}

static int64_t _pgas_sub(hpx_addr_t lhs, hpx_addr_t rhs, uint32_t bsize) {
  bool clhs = _check_cyclic(lhs, &bsize);
  bool crhs = _check_cyclic(rhs, &bsize);
  DEBUG_IF (clhs != crhs) {
    dbg_error("cannot compare addresses between different allocations.\n");
  }
  return pgas_gva_sub(lhs.offset, rhs.offset, here->ranks, bsize);
}

static hpx_addr_t _pgas_add(hpx_addr_t gva, int64_t bytes, uint32_t bsize) {
  _check_cyclic(gva, &bsize);
  hpx_addr_t gva1 = HPX_ADDR_INIT(pgas_gva_add(gva.offset, bytes, here->ranks,
                                               bsize), 0, 0);
  return gva1;
}

static hpx_addr_t _pgas_lva_to_gva(void *lva) {
  uint64_t goffset = heap_offset_of(global_heap, lva);
  hpx_addr_t gva = HPX_ADDR_INIT(pgas_gva_from_goffset(here->rank, goffset,
                                                       here->ranks), 0, 0);
  return gva;
}

static bool _pgas_try_pin(const hpx_addr_t addr, void **local) {
  pgas_gva_t gva = addr.offset;
  uint32_t l = pgas_gva_locality_of(gva, here->ranks);
  if (l != here->rank)
    return false;

  if (local) {
    uint64_t goffset = pgas_gva_goffset_of(gva, here->ranks);
    *local = heap_offset_to_local(global_heap, goffset);
  }

  return true;
}

static void *_pgas_gva_to_lva(hpx_addr_t addr) {
  void *local;
  bool is_local = _pgas_try_pin(addr, &local);
  DEBUG_IF (!is_local) {
    dbg_error("%lu is not local to %u\n", addr.offset, here->rank);
  }
  return local;
}

static void _pgas_unpin(const hpx_addr_t addr) {
  DEBUG_IF(!_pgas_try_pin(addr, NULL)) {
    dbg_error("%lu is not local to %u\n", addr.offset, here->rank);
  }
}

static hpx_addr_t _pgas_gas_cyclic_alloc(size_t n, uint32_t bsize) {
  hpx_addr_t addr;
  alloc_handler_args_t args = {
    .n = n,
    .bsize = bsize
  };
  int e = hpx_call_sync(HPX_THERE(0), act_pgas_cyclic_alloc_handler,
                        &args, sizeof(args), &addr, sizeof(addr));
  dbg_check(e, "Failed to call pgas_cyclic_alloc_handler.\n");
  return addr;
}

static hpx_addr_t _pgas_gas_cyclic_calloc(size_t n, uint32_t bsize) {
  hpx_addr_t addr;
  alloc_handler_args_t args = {
    .n = n,
    .bsize = bsize
  };
  int e = hpx_call_sync(HPX_THERE(0), act_pgas_cyclic_alloc_handler,
                        &args, sizeof(args), &addr, sizeof(addr));
  dbg_check(e, "Failed to call pgas_cyclic_calloc_handler.\n");
  return addr;
}

static hpx_addr_t _pgas_gas_alloc(uint32_t bytes) {
  void *lva = pgas_global_malloc(bytes);
  assert(lva && heap_contains(global_heap, lva));
  uint64_t goffset = heap_offset_of(global_heap, lva);
  pgas_gva_t gva = pgas_gva_from_goffset(here->rank, goffset, here->ranks);
  hpx_addr_t addr = HPX_ADDR_INIT(gva, 0, 0);
  return addr;
}

static void _pgas_gas_free(hpx_addr_t addr, hpx_addr_t sync) {
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
  .locality_of   = _pgas_locality_of,
  .offset_of     = _pgas_offset_of,
  .phase_of      = _pgas_phase_of,
  .sub           = _pgas_sub,
  .add           = _pgas_add,
  .lva_to_gva    = _pgas_lva_to_gva,
  .gva_to_lva    = _pgas_gva_to_lva,
  .try_pin       = _pgas_try_pin,
  .unpin         = _pgas_unpin,
  .cyclic_alloc  = _pgas_gas_cyclic_alloc,
  .cyclic_calloc = _pgas_gas_cyclic_calloc,
  .local_alloc   = _pgas_gas_alloc,
  .free          = _pgas_gas_free

};

gas_class_t *gas_pgas_new(size_t heap_size, boot_class_t *boot,
                          struct transport_class *transport) {
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
