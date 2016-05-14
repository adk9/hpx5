// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2016, Trustees of Indiana University,
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

#include <stdlib.h>
#include <string.h>
#include <libhpx/action.h>
#include <libhpx/config.h>
#include <libhpx/debug.h>
#include <libhpx/lco.h>
#include <libhpx/locality.h>
#include <libhpx/memory.h>
#include <libhpx/network.h>
#include <libhpx/scheduler.h>
#include <libhpx/worker.h>
#include <libsync/locks.h>
#include "agas.h"
#include "btt.h"

static int _insert_block_handler(int n, void *args[], size_t sizes[]) {
  agas_t *agas = (agas_t*)here->gas;
  hpx_addr_t dst = hpx_thread_current_target();
  gva_t dgva = { .addr = dst };

  uint32_t owner;
  btt_get_owner(agas->btt, dgva, &owner);
  if (here->rank != owner) {
    return HPX_RESEND;
  }

  dbg_assert(args[0] && sizes[0]);
  hpx_addr_t *src  = args[1];
  uint32_t   *attr = args[2];

  size_t bsize = sizes[0];
  char *lva = malloc(bsize);
  memcpy(lva, args[0], bsize);

  gva_t sgva = { .addr = *src };
  btt_upsert(agas->btt, sgva, here->rank, lva, 1, *attr);
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_MARSHALLED | HPX_VECTORED, _insert_block,
                     _insert_block_handler, HPX_INT, HPX_POINTER, HPX_POINTER);

/// Invalidate the remote block mapping. This action blocks until it
/// can safely invalidate the block.
static HPX_ACTION_DECL(_agas_invalidate_mapping);
static int _agas_invalidate_mapping_handler(hpx_addr_t dst, int to) {
  // unnecessary to move to the same locality
  if (here->rank == to) {
    return HPX_SUCCESS;
  }

  agas_t *agas = (agas_t*)here->gas;
  hpx_addr_t src = hpx_thread_current_target();
  gva_t gva = { .addr = src };

  // instrument the move event
  EVENT_GAS_MOVE(src, HPX_HERE, dst);

  uint32_t owner;
  btt_get_owner(agas->btt, gva, &owner);
  if (here->rank != owner) {
    return HPX_RESEND;
  }

  void *block = NULL;
  uint32_t attr;
  int e = btt_try_move(agas->btt, gva, to, &block, &attr);
  if (e != HPX_SUCCESS) {
    log_error("failed to invalidate remote mapping.\n");
    return e;
  }

  size_t bsize = UINT64_C(1) << gva.bits.size;
  e = hpx_call_cc(dst, _insert_block, block, bsize, &src, sizeof(src), &attr,
                  sizeof(attr));

  // otherwise only free if the block is not at its home
  if (gva.bits.home != here->rank) {
    free(block);
  }

  return e;
}
static LIBHPX_ACTION(HPX_DEFAULT, 0, _agas_invalidate_mapping,
                     _agas_invalidate_mapping_handler, HPX_ADDR, HPX_INT);

static int _agas_move_handler(hpx_addr_t src) {
  int rank = here->rank;
  hpx_addr_t dst = hpx_thread_current_target();
  return hpx_call_cc(src, _agas_invalidate_mapping, &dst, &rank);
}
static LIBHPX_ACTION(HPX_DEFAULT, 0, _agas_move, _agas_move_handler, HPX_ADDR);

void agas_move(void *gas, hpx_addr_t src, hpx_addr_t dst, hpx_addr_t sync) {
  agas_t *agas = gas;
  libhpx_network_t net = here->config->network;
  if (net == HPX_NETWORK_SMP) {
    log_dflt("AGAS move not supported for network %s\n",
             HPX_NETWORK_TO_STRING[net]);
    hpx_lco_set(sync, 0, NULL, HPX_NULL, HPX_NULL);
    return;
  }

  gva_t gva = { .addr = dst };
  uint32_t owner;
  bool found = btt_get_owner(agas->btt, gva, &owner);
  if (found) {
    hpx_call(src, _agas_invalidate_mapping, sync, &dst, &owner);
    return;
  }

  hpx_call(dst, _agas_move, sync, &src);
}
