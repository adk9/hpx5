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

static int _agas_invalidate_mapping_handler(int rank) {
  agas_t *agas = (agas_t*)here->gas;
  hpx_addr_t src = hpx_thread_current_target();
  gva_t gva = { .addr = src };
  size_t bsize = UINT64_C(1) << gva.bits.size;

  void *block = NULL;
  int e = btt_try_move(agas->btt, gva, rank, &block);
  if (e != HPX_SUCCESS) {
    log_error("failed to invalidate remote mapping.\n");
    return e;
  }
  hpx_thread_continue(bsize, block);
  // TODO: free the src block here.
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, 0, _agas_invalidate_mapping,
                     _agas_invalidate_mapping_handler, HPX_INT);

static int _agas_move_handler(hpx_addr_t src) {
  agas_t *agas = (agas_t*)here->gas;
  gva_t gva = { .addr = src };
  size_t bsize = UINT64_C(1) << gva.bits.size;
  hpx_addr_t local = hpx_lco_future_new(bsize);

  int rank = here->rank;
  int e = hpx_call(src, _agas_invalidate_mapping, local, &rank);
  if (e != HPX_SUCCESS) {
    log_error("failed agas move operation.\n");
    return e;
  }

  void *buf;
  bool stack_allocated = true;
  if (hpx_thread_can_alloca(bsize) >= HPX_PAGE_SIZE) {
    buf = alloca(bsize);
  } else {
    stack_allocated = false;
    buf = malloc(bsize);
  }

  e = hpx_lco_get(local, bsize, buf);
  if (e != HPX_SUCCESS) {
    log_error("failed agas move operation.\n");
    return e;
  }

  btt_insert(agas->btt, gva, here->rank, buf, 1);

  hpx_lco_delete(local, HPX_NULL);
  if (!stack_allocated) {
    free(buf);
  }
  return HPX_SUCCESS;
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

  hpx_addr_t rsync = hpx_lco_future_new(0);
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
