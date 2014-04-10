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

#include "libsync/sync.h"
#include "hpx/hpx.h"
#include "libhpx/debug.h"
#include "sbrk.h"

static SYNC_ATOMIC(uint32_t _next_block);       // the next global block id

static hpx_action_t _gasbrk = 0;

/// The action that performs the global sbrk.
static int _gasbrk_action(size_t *args) {
  if (!hpx_addr_eq(HPX_HERE, HPX_THERE(0)))
    return dbg_error("Centralized allocation expects rank 0");

  // bump the next block id by the required number of blocks---always bump a
  // ranks-aligned value
  int ranks = hpx_get_num_ranks();
  size_t n = *args + (*args % ranks);
  int next = sync_fadd(&_next_block, n, SYNC_ACQ_REL);
  if (UINT32_MAX - next < n) {
    dbg_error("rank out of blocks for allocation size %lu\n", n);
    hpx_abort();
  }

  // return the base block id of the allocated blocks, the caller can use this
  // to initialize block addresses
  hpx_thread_continue(sizeof(next), &next);
}

uint32_t gas_sbrk(size_t n) {
  uint32_t base_id;
  hpx_addr_t f = hpx_lco_future_new(sizeof(base_id));
  hpx_call(HPX_THERE(0), _gasbrk, &n, sizeof(n), f);
  hpx_lco_get(f, &base_id, sizeof(base_id));
  hpx_lco_delete(f, HPX_NULL);
  return base_id;
}

void gas_sbrk_init(uint32_t ranks) {
  sync_store(&_next_block, ranks, SYNC_RELEASE);
  _gasbrk = HPX_REGISTER_ACTION(_gasbrk_action);
}
