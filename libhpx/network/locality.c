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
#include <stdio.h>
#include <stdbool.h>
#include "libhpx/btt.h"
#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/network.h"
#include "libhpx/scheduler.h"
#include "libhpx/system.h"

locality_t *here = NULL;

hpx_action_t locality_shutdown          = 0;
hpx_action_t locality_global_sbrk       = 0;
hpx_action_t locality_alloc_blocks      = 0;
hpx_action_t locality_gas_alloc         = 0;
hpx_action_t locality_gas_move          = 0;
hpx_action_t locality_gas_acquire       = 0;
hpx_action_t locality_gas_forward       = 0;
hpx_action_t locality_call_continuation = 0;



/// The action that performs a shared global allocation for a rank.
static int _alloc_blocks_action(uint32_t *args) {
  uint32_t base_id = args[0];
  uint32_t n = args[1];
  uint32_t size = args[2];


  // Update the value of global_sbrk locally so that we can check if
  // we are out of memory for local GAS allocations without having to
  // request the current global_sbrk value from the root locality.
  if (here->rank != 0)
    sync_fadd(&here->global_sbrk, base_id + n * here->ranks, SYNC_ACQ_REL);

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
  // Bump the next block id by the required number of blocks---always
  // bump a ranks-aligned value
  size_t n = *args + (here->ranks - (*args % here->ranks));
  uint32_t next = sync_fadd(&here->global_sbrk, n, SYNC_ACQ_REL);
  if (UINT32_MAX - next < n)
    return dbg_error("gas: rank out of blocks for allocation size %lu.\n", n);

  // return the base block id of the allocated blocks, the caller can use this
  // to initialize block addresses
  hpx_thread_continue(sizeof(next), &next);
  return HPX_SUCCESS;
}


static int _gas_alloc_action(uint32_t *args) {
  uint32_t bytes = *args;
  hpx_addr_t addr = hpx_gas_alloc(bytes);
  hpx_thread_continue(sizeof(addr), &addr);
  return HPX_SUCCESS;
}

/// The action that shuts down the HPX scheduler.
static int _shutdown_action(void *args) {
  scheduler_shutdown(here->sched);
  return HPX_SUCCESS;
}


/// Updates a mapping to a forward, and continues the block data.
static int _gas_acquire_action(uint32_t *rank) {
  hpx_addr_t addr = hpx_thread_current_target();
  void *block = NULL;
  if (btt_forward(here->btt, addr, *rank, &block))
    hpx_thread_continue_cleanup(addr.block_bytes, block, free, block);
  else
    hpx_thread_exit(HPX_LCO_ERROR);
}


/// Updates a mapping to a forward.
static int _gas_forward_action(locality_gas_forward_args_t *args) {
  btt_forward(here->btt, args->addr, args->rank, NULL);
  return HPX_SUCCESS;
}


static int _gas_move_action(hpx_addr_t *args) {
  hpx_addr_t src = *args;

  int size = src.block_bytes;
  uint32_t rank = here->rank;

  // 1. allocate local memory for the block.
  char *block = malloc(size);
  assert(block);

  // 2. invalidate the block mapping at the source locality.
  hpx_status_t status = hpx_call_sync(src, locality_gas_acquire, &rank, sizeof(rank),
                                      block, size);
  if (status != HPX_SUCCESS) {
    dbg_log_net("locality: failed move operation.\n");
    hpx_thread_exit(status);
  }

  // 3. If the invalidate was successful, insert an entry into the local block
  //    translation table
  btt_insert(here->btt, src, block);
  return status;
}


static int _call_cont_action(locality_cont_args_t *args) {
  hpx_addr_t target = hpx_thread_current_target();
  if (!hpx_gas_try_pin(target, NULL))
    return HPX_RESEND;

  uint32_t size = hpx_thread_current_args_size() - sizeof(args->status) - sizeof(args->action);
  // handle status here: args->status;
  return hpx_call(target, args->action, args->data, size, HPX_NULL);
}


static HPX_CONSTRUCTOR void _init_actions(void) {
  locality_shutdown          = HPX_REGISTER_ACTION(_shutdown_action);
  locality_global_sbrk       = HPX_REGISTER_ACTION(_global_sbrk_action);
  locality_alloc_blocks      = HPX_REGISTER_ACTION(_alloc_blocks_action);
  locality_gas_alloc         = HPX_REGISTER_ACTION(_gas_alloc_action);
  locality_gas_move          = HPX_REGISTER_ACTION(_gas_move_action);
  locality_gas_acquire       = HPX_REGISTER_ACTION(_gas_acquire_action);
  locality_gas_forward       = HPX_REGISTER_ACTION(_gas_forward_action);
  locality_call_continuation = HPX_REGISTER_ACTION(_call_cont_action);
}

/// ----------------------------------------------------------------------------
/// Local allocation is done from our designated local block. Allocation is
/// always done to 8 byte alignment. Here we're using a simple sbrk allocator
/// with no free functionality for now.
/// ----------------------------------------------------------------------------
hpx_addr_t
locality_malloc(size_t bytes) {
  bytes += bytes % 8;
  uint32_t offset = sync_fadd(&here->local_sbrk, bytes, SYNC_ACQ_REL);
  if (UINT32_MAX - offset < bytes)
    dbg_error("locality: exhausted local allocation limit with %lu-byte allocation.\n",
              bytes);

  return hpx_addr_add(HPX_HERE, offset);
}
