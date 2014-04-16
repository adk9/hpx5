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
#include "config.h"
#endif


/// ----------------------------------------------------------------------------
/// Implement the locality actions.
/// ----------------------------------------------------------------------------
#include <stdlib.h>
#include <stdbool.h>
#include "libhpx/btt.h"
#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/network.h"
#include "libhpx/system.h"

locality_t *here = NULL;

hpx_action_t locality_shutdown           = 0;
hpx_action_t locality_global_sbrk        = 0;
hpx_action_t locality_alloc_blocks       = 0;
hpx_action_t locality_move_block         = 0;
static hpx_action_t _locality_invalidate = 0;


/// The action that performs a global allocation for a rank.
static int _alloc_blocks_action(uint32_t *args) {
  uint32_t base_id = args[0];
  uint32_t n = args[1];
  uint32_t size = args[2];

  // Insert all of the mappings (always block cyclic allocations). We use
  // distinct allocations for each block because we want to be able to move
  // blocks dynamically, and if they are all in one big malloc then we can't
  // free them individually.
  for (int i = 0; i < n; ++i) {
    uint32_t block_id = base_id + i * here->ranks + here->rank;
    hpx_addr_t addr = hpx_addr_init(0, block_id, size);
    char *block = malloc(size);
    assert(block);
    btt_insert(here->btt, addr, block);
  }

  return HPX_SUCCESS;
}


/// The action that performs the global sbrk.
static int _global_sbrk_action(size_t *args) {
  // Bump the next block id by the required number of blocks---always bump a
  // ranks-aligned value
  size_t n = *args + (*args % here->ranks);
  int next = sync_fadd(&here->global_sbrk, n, SYNC_ACQ_REL);
  if (UINT32_MAX - next < n) {
    dbg_error("rank out of blocks for allocation size %lu\n", n);
    hpx_abort();
  }

  // return the base block id of the allocated blocks, the caller can use this
  // to initialize block addresses
  hpx_thread_continue(sizeof(next), &next);
}


static int _shutdown_action(void *args) {
  network_shutdown(here->network);
  system_shutdown(0);
  return HPX_SUCCESS;
}


/// The action that invalidates a block mapping on a given locality.
static int _invalidate_action(hpx_addr_t *args) {
  hpx_addr_t addr = hpx_thread_current_target();
  void *base = btt_invalidate(here->btt, addr);

  // Continue the actual bytes from the block, so that the caller can remap the
  // block.
  if (base)
    hpx_thread_continue_cleanup(addr.block_bytes, base, free, base);
  else
    hpx_thread_continue(0, NULL);
}


static int _move_block_action(hpx_addr_t *args) {
  hpx_addr_t src = *args;

  void *rblock = NULL;
  int size = src.block_bytes;

  // 1. invalidate the block mapping at the source locality.
  hpx_addr_t done = hpx_lco_future_new(size);
  hpx_call(src, _locality_invalidate, NULL, 0, done);

  // 2. allocate local memory for the block.
  char *block = malloc(size);
  assert(block);

  hpx_lco_get(done, rblock, size);
  hpx_lco_delete(done, HPX_NULL);
  if (!rblock) {
    free(block);
    hpx_thread_continue(0, NULL);
  }

  // 3. Insert an entry into the block translation table.
  btt_insert(here->btt, src, block);
  hpx_thread_continue(0, NULL);
}

static HPX_CONSTRUCTOR void _init_actions(void) {
  locality_shutdown     = HPX_REGISTER_ACTION(_shutdown_action);
  locality_global_sbrk  = HPX_REGISTER_ACTION(_global_sbrk_action);
  locality_alloc_blocks = HPX_REGISTER_ACTION(_alloc_blocks_action);
  locality_move_block   = HPX_REGISTER_ACTION(_move_block_action);
  _locality_invalidate  = HPX_REGISTER_ACTION(_invalidate_action);
}
