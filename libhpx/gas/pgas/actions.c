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

#include <string.h>
#include "libhpx/locality.h"
#include "gva.h"
#include "heap.h"
#include "pgas.h"

hpx_action_t pgas_cyclic_alloc = 0;
hpx_action_t pgas_cyclic_calloc = 0;
hpx_action_t pgas_memset = 0;
hpx_action_t pgas_free = 0;
hpx_action_t pgas_set_csbrk = 0;

/// Allocate from the cyclic space.
///
/// This is performed at the single cyclic server node (usually rank 0), and
/// doesn't need to be broadcast because the server controls this for
/// everyone. All global cyclic allocations are rooted at rank 0.
///
/// @param        n The number of blocks to allocate.
/// @param    bsize The block size for this allocation.
///
/// @returns The base address of the global allocation.
hpx_addr_t pgas_cyclic_alloc_sync(size_t n, uint32_t bsize) {
  const uint64_t blocks_per_locality = pgas_n_per_locality(n, here->ranks);
  const uint32_t padded_bsize = pgas_fit_log2_32(bsize);
  const uint64_t offset = heap_csbrk(global_heap, blocks_per_locality,
                                     padded_bsize);

  DEBUG_IF (true) {
    // during DEBUG execution we broadcast the csbrk to the system to make sure
    // that people can do effective cyclic vs. gas allocations
    hpx_addr_t sync = hpx_lco_future_new(0);
    hpx_bcast(pgas_set_csbrk, &offset, sizeof(offset), sync);
    hpx_lco_wait(sync);
    hpx_lco_delete(sync, HPX_NULL);
  }

  return pgas_offset_to_gva(here->rank, offset);
}

/// Allocate zeroed memory from the cyclic space.
///
/// This is performed at the single cyclic server node (usually rank 0) and is
/// broadcast to all of the ranks in the system using hpx_bcast(). It waits for
/// the broadcast to finish before returning. All global cyclic allocations are
/// rooted at rank 0.
///
/// @param        n The number of blocks to allocate.
/// @param    bsize The block size for this allocation.
///
/// @returns The base address of the global allocation.
hpx_addr_t pgas_cyclic_calloc_sync(size_t n, uint32_t bsize) {
  const uint32_t ranks = here->ranks;
  const uint64_t blocks_per_locality = pgas_n_per_locality(n, ranks);
  const uint32_t padded_bsize = pgas_fit_log2_32(bsize);
  const size_t offset = heap_csbrk(global_heap, blocks_per_locality,
                                   padded_bsize);

  pgas_memset_args_t args = {
    .offset = offset,
    .value = 0,
    .length = blocks_per_locality * padded_bsize
  };

  DEBUG_IF (true) {
    // during DEBUG execution we broadcast the csbrk to the system to make sure
    // that people can do effective cyclic vs. gas allocations
    //
    // NB: we're already broadcasting memset, we could just do it then...?
    hpx_addr_t sync = hpx_lco_future_new(0);
    hpx_bcast(pgas_set_csbrk, &offset, sizeof(offset), sync);
    hpx_lco_wait(sync);
    hpx_lco_delete(sync, HPX_NULL);
  }

  hpx_addr_t sync = hpx_lco_future_new(0);
  hpx_bcast(pgas_memset, &args, sizeof(args), sync);
  hpx_lco_wait(sync);
  hpx_lco_delete(sync, HPX_NULL);
  return pgas_offset_to_gva(here->rank, offset);
}


/// This is the hpx_call_* target for a cyclic allocation.
static int _pgas_cyclic_alloc_handler(pgas_alloc_args_t *args) {
  hpx_addr_t addr = pgas_cyclic_alloc_sync(args->n, args->bsize);
  HPX_THREAD_CONTINUE(addr);
}

/// This is the hpx_call_* target for cyclic zeroed allocation.
static int _pgas_cyclic_calloc_handler(pgas_alloc_args_t *args) {
  hpx_addr_t addr = pgas_cyclic_calloc_sync(args->n, args->bsize);
  HPX_THREAD_CONTINUE(addr);
}

/// This is the hpx_call_* target for memset, used in calloc broadcast.
static int _pgas_memset_handler(pgas_memset_args_t *args) {
  void *dest = heap_offset_to_lva(global_heap, args->offset);
  memset(dest, args->value, args->length);
  return HPX_SUCCESS;
}

static int _pgas_free_handler(void *UNUSED) {
  void *local = NULL;
  hpx_addr_t addr = hpx_thread_current_target();
  if (!pgas_try_pin(addr, &local)) {
    dbg_error("failed to translate an address during free.\n");
    return HPX_ERROR;
  }
  pgas_global_free(local);
  return HPX_SUCCESS;
}

static int _pgas_set_csbrk_handler(size_t *offset) {
  int e = heap_set_csbrk(global_heap, *offset);
  dbg_check(e, "cyclic allocation ran out of memory at rank %u", here->rank);
  return e;
}

void pgas_register_actions(void) {
  pgas_cyclic_alloc = HPX_REGISTER_ACTION(_pgas_cyclic_alloc_handler);
  pgas_cyclic_calloc = HPX_REGISTER_ACTION(_pgas_cyclic_calloc_handler);
  pgas_memset = HPX_REGISTER_ACTION(_pgas_memset_handler);
  pgas_free = HPX_REGISTER_ACTION(_pgas_free_handler);
  pgas_set_csbrk = HPX_REGISTER_ACTION(_pgas_set_csbrk_handler);
}

static void HPX_CONSTRUCTOR _register(void) {
  pgas_register_actions();
}
