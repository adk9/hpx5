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
#include "libhpx/btt.h"
#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/network.h"
#include "libhpx/system.h"

locality_t *here = NULL;

hpx_action_t locality_shutdown = 0;
hpx_action_t locality_global_sbrk = 0;
hpx_action_t locality_alloc_blocks = 0;


/// The action that performs a global allocation for a rank.
static int _alloc_blocks_action(uint32_t *args) {
  uint32_t base_id = args[0];
  uint32_t n = args[1];
  uint32_t size = args[2];

  // Map a contiguous local memory region for the blocks.
  uint64_t bytes = n * size;
  char *blocks = malloc(bytes);
  assert(blocks);

  // Insert all of the mappings (always block cyclic allocations).
  for (int i = 0; i < n; ++i) {
    uint32_t block_id = base_id + i * here->ranks + here->rank;
    hpx_addr_t addr = hpx_addr_init(0, block_id, size);
    char *block = blocks + i * size;
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


static HPX_CONSTRUCTOR void _init_actions(void) {
  locality_shutdown = HPX_REGISTER_ACTION(_shutdown_action);
  locality_global_sbrk = HPX_REGISTER_ACTION(_global_sbrk_action);
  locality_alloc_blocks = HPX_REGISTER_ACTION(_alloc_blocks_action);
}
