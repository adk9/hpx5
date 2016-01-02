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

#include <stdlib.h>
#include <string.h>
#include <libhpx/action.h>
#include <libhpx/config.h>
#include <libhpx/debug.h>
#include <libhpx/locality.h>
#include <libhpx/network.h>
#include <libhpx/scheduler.h>
#include "agas.h"
#include "btt.h"
#include "../parcel/emulation.h"

static int _insert_block_handler(int n, void *args[], size_t sizes[]) {
  agas_t *agas = (agas_t*)here->gas;
  void *block = args[0];
  hpx_addr_t *src = args[1];
  uint32_t *attr = args[2];

  gva_t gva = { .addr = *src };
  btt_insert(agas->btt, gva, here->rank, block, 1, *attr);
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_MARSHALLED | HPX_VECTORED, _insert_block,
                     _insert_block_handler, HPX_INT, HPX_POINTER, HPX_POINTER);

/// Invalidate the remote block mapping. This action blocks until it
/// can safely invalide the block.
static int _agas_invalidate_mapping_handler(hpx_addr_t dst, int rank) {
  agas_t *agas = (agas_t*)here->gas;
  hpx_addr_t src = hpx_thread_current_target();
  gva_t gva = { .addr = src };
  size_t bsize = UINT64_C(1) << gva.bits.size;

  void *block = NULL;
  uint32_t attr;
  int e = btt_try_move(agas->btt, gva, rank, &block, &attr);
  if (e != HPX_SUCCESS) {
    log_error("failed to invalidate remote mapping.\n");
    return e;
  }

  void (*_cleanup)(void*) = NULL;
  void *_env = NULL;

  // since rank 0 maintains the cyclic global address space, we cannot
  // free cyclic blocks on rank 0.
  if (!(gva.bits.cyclic && here->rank == 0)) {
    _cleanup = free;
    _env = block;
  }

  e = hpx_call_cc(dst, _insert_block, &block, bsize, &src, sizeof(src), &attr,
                  sizeof(attr));
  _cleanup(_env);
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

void
agas_move(void *gas, hpx_addr_t src, hpx_addr_t dst, hpx_addr_t sync) {
  libhpx_network_t net = here->config->network;
  if (net != HPX_NETWORK_ISIR) {
    hpx_lco_set(sync, 0, NULL, HPX_NULL, HPX_NULL);
  }
  hpx_call(dst, _agas_move, sync, &src);
}

static int _agas_lco_set_handler(int src, uint64_t command) {
  hpx_addr_t lco = command;
  hpx_lco_set(lco, 0, NULL, HPX_NULL, HPX_NULL);
  return HPX_SUCCESS;
}
static COMMAND_DEF(_agas_lco_set, _agas_lco_set_handler);

int
agas_memput(void *gas, hpx_addr_t to, const void *from, size_t n,
            hpx_addr_t lsync, hpx_addr_t rsync) {
  if (!n) {
    hpx_lco_set(lsync, 0, NULL, HPX_NULL, HPX_NULL);
    hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
    return HPX_SUCCESS;
  }

  agas_t *agas = gas;
  gva_t gva = { .addr = to };
  void *lto = NULL;
  if (btt_try_pin(agas->btt, gva, &lto)) {
    memcpy(lto, from, n);
    btt_unpin(agas->btt, gva);
    hpx_lco_set(lsync, 0, NULL, HPX_NULL, HPX_NULL);
    hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
    return HPX_SUCCESS;
  }

  hpx_action_t lop = lsync ? _agas_lco_set : HPX_ACTION_NULL;
  if (rsync) {
    return network_pwc(here->network, to, from, n, lop, lsync,
                       _agas_lco_set, rsync);
  }
  else {
    return network_put(here->network, to, from, n, lop, lsync);
  }
}

int
agas_memput_lsync(void *gas, hpx_addr_t to, const void *from, size_t n,
                  hpx_addr_t rsync) {
  if (!n) {
    hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
    return HPX_SUCCESS;
  }

  agas_t *agas = gas;
  gva_t gva = { .addr = to };
  void *lto = NULL;
  if (btt_try_pin(agas->btt, gva, &lto)) {
    memcpy(lto, from, n);
    btt_unpin(agas->btt, gva);
    hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
    return HPX_SUCCESS;
  }

  hpx_addr_t lsync = hpx_lco_future_new(0);
  int e = HPX_SUCCESS;
  if (rsync) {
    e = network_pwc(here->network, to, from, n, _agas_lco_set, lsync,
                        _agas_lco_set, rsync);
    dbg_check(e, "failed network pwc\n");
  }
  else {
    e = network_put(here->network, to, from, n, _agas_lco_set, lsync);
    dbg_check(e, "failed network put\n");
  }
  e = hpx_lco_wait(lsync);
  dbg_check(e, "lsync LCO reported error\n");
  hpx_lco_delete(lsync, HPX_NULL);              // TODO: async safe?
  return HPX_SUCCESS;
}

int
agas_memput_rsync(void *gas, hpx_addr_t to, const void *from, size_t n) {
  if (!n) {
    return HPX_SUCCESS;
  }

  agas_t *agas = gas;
  gva_t gva = { .addr = to };
  void *lto = NULL;
  if (btt_try_pin(agas->btt, gva, &lto)) {
    memcpy(lto, from, n);
    btt_unpin(agas->btt, gva);
    return HPX_SUCCESS;
  }

  hpx_addr_t rsync = hpx_lco_future_new(4);
  int e = network_pwc(here->network, to, from, n, HPX_ACTION_NULL, HPX_NULL,
                      _agas_lco_set, rsync);
  dbg_check(e, "failed network pwc\n");
  e = hpx_lco_wait(rsync);
  dbg_check(e, "lsync LCO reported error\n");
  hpx_lco_delete(rsync, HPX_NULL);              // TODO: async safe?
  return HPX_SUCCESS;
}

int
agas_memget(void *gas, void *to, hpx_addr_t from, size_t n, hpx_addr_t lsync) {
  if (!n) {
    hpx_lco_set(lsync, 0, NULL, HPX_NULL, HPX_NULL);
    return HPX_SUCCESS;
  }

  agas_t *agas = gas;
  gva_t gva = { .addr = from };
  void *lfrom = NULL;
  if (btt_try_pin(agas->btt, gva, &lfrom)) {
    memcpy(to, lfrom, n);
    btt_unpin(agas->btt, gva);
    hpx_lco_set(lsync, 0, NULL, HPX_NULL, HPX_NULL);
    return HPX_SUCCESS;
  }
  else {
    return network_get(here->network, to, from, n, _agas_lco_set, lsync);
  }
  return HPX_SUCCESS;
}

int
agas_memget_lsync(void *gas, void *to, hpx_addr_t from, size_t n) {
  if (!n) {
    return HPX_SUCCESS;
  }

  agas_t *agas = gas;
  gva_t gva = { .addr = from };
  void *lfrom = NULL;
  if (btt_try_pin(agas->btt, gva, &lfrom)) {
    memcpy(to, lfrom, n);
    btt_unpin(agas->btt, gva);
    return HPX_SUCCESS;
  }
  else {
    hpx_addr_t lsync = hpx_lco_future_new(0);
    int e = network_get(here->network, to, from, n, _agas_lco_set, lsync);
    dbg_check(e, "failed network get\n");
    e = hpx_lco_wait(lsync);
    dbg_check(e, "lsync LCO reported error\n");
    hpx_lco_delete(lsync, HPX_NULL);            // async okay?
    return HPX_SUCCESS;
  }
}

int
agas_memcpy(void *gas, hpx_addr_t to, hpx_addr_t from, size_t size,
            hpx_addr_t sync) {
  if (!size) {
    return HPX_SUCCESS;
  }

  void *lto;
  void *lfrom;

  if (!hpx_gas_try_pin(to, &lto)) {
    return parcel_memcpy(to, from, size, sync);
  }
  else if (!hpx_gas_try_pin(from, &lfrom)) {
    hpx_gas_unpin(to);
    return parcel_memcpy(to, from, size, sync);
  }

  memcpy(lto, lfrom, size);
  hpx_gas_unpin(to);
  hpx_gas_unpin(from);
  hpx_lco_set(sync, 0, NULL, HPX_NULL, HPX_NULL);
  return HPX_SUCCESS;
}

int
agas_memcpy_sync(void *gas, hpx_addr_t to, hpx_addr_t from, size_t size) {
  int e = HPX_SUCCESS;
  if (!size) {
    return e;
  }

  hpx_addr_t sync = hpx_lco_future_new(0);
  if (sync == HPX_NULL) {
    log_error("could not allocate an LCO.\n");
    return HPX_ENOMEM;
  }

  e = agas_memcpy(gas, to, from, size, sync);

  if (HPX_SUCCESS != hpx_lco_wait(sync)) {
    dbg_error("failed agas_memcpy_sync\n");
  }

  hpx_lco_delete(sync, HPX_NULL);
  return e;
}
