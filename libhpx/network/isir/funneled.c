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
#include <hpx/builtins.h>
#include <libsync/queues.h>

#include <libhpx/debug.h>
#include <libhpx/gas.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/padding.h>
#include <libhpx/parcel.h>

#include "emulate_pwc.h"
#include "irecv_buffer.h"
#include "isend_buffer.h"
#include "isir.h"
#include "xport.h"

typedef struct {
  network_t       vtable;
  gas_t             *gas;
  isir_xport_t    *xport;
  volatile int     flush;
  PAD_TO_CACHELINE(sizeof(network_t) + sizeof(gas_t*) + sizeof(isir_xport_t*) +
                   sizeof(int));
  two_lock_queue_t sends;
  two_lock_queue_t recvs;
  isend_buffer_t  isends;
  irecv_buffer_t  irecvs;
} _funneled_t;

/// Transfer any parcels in the funneled sends queue into the isends buffer.
static void
_send_all(_funneled_t *network) {
  hpx_parcel_t *p = NULL;
  while ((p = sync_two_lock_queue_dequeue(&network->sends))) {
    isend_buffer_append(&network->isends, p, HPX_NULL);
  }
}

/// Delete a funneled network.
static void
_funneled_delete(void *network) {
  dbg_assert(network);

  _funneled_t *isir = network;

  // flush sends if we're supposed to
  if (isir->flush) {
    _send_all(isir);
    isend_buffer_flush(&isir->isends);
  }

  isend_buffer_fini(&isir->isends);
  irecv_buffer_fini(&isir->irecvs);

  hpx_parcel_t *p = NULL;
  while ((p = sync_two_lock_queue_dequeue(&isir->sends))) {
    hpx_parcel_release(p);
  }
  while ((p = sync_two_lock_queue_dequeue(&isir->recvs))) {
    hpx_parcel_release(p);
  }

  sync_two_lock_queue_fini(&isir->sends);
  sync_two_lock_queue_fini(&isir->recvs);

  isir->xport->delete(isir->xport);
  free(isir);
}

static int
_funneled_send(void *network, hpx_parcel_t *p) {
  _funneled_t *isir = network;
  sync_two_lock_queue_enqueue(&isir->sends, p);
  return LIBHPX_OK;
}

static int
_funneled_command(void *network, hpx_addr_t locality, hpx_action_t op,
                  uint64_t args) {
  return hpx_xcall(locality, op, HPX_NULL, here->rank, args);
}

static int
_funneled_pwc(void *network,
              hpx_addr_t to, const void *from, size_t n,
              hpx_action_t lop, hpx_addr_t laddr,
              hpx_action_t rop, hpx_addr_t raddr) {
  dbg_assert(lop || !laddr); // !lop => !lsync

  hpx_addr_t lsync = HPX_NULL;
  hpx_addr_t rsync = HPX_NULL;
  if (lop && laddr) {
    lsync = hpx_lco_future_new(0);
    dbg_assert(lsync);
    int e = hpx_call_when_with_continuation(lsync, laddr, lop, lsync,
                                            hpx_lco_delete_action, &here->rank,
                                            &laddr);
    dbg_check(e, "failed to chain parcel\n");
  }

  if (rop && raddr) {
    rsync = hpx_lco_future_new(0);
    dbg_assert(rsync);
    int e = hpx_call_when_with_continuation(rsync, raddr, rop, rsync,
                                            hpx_lco_delete_action, &here->rank,
                                            &raddr);
    dbg_check(e, "failed to chain parcel\n");
  }

  return hpx_call_async(to, isir_emulate_pwc, lsync, rsync, from, n);
}

static int
_funneled_put(void *network, hpx_addr_t to, const void *from, size_t n,
              hpx_action_t lop, hpx_addr_t laddr) {
  hpx_action_t rop = HPX_ACTION_NULL;
  hpx_addr_t raddr = HPX_NULL;
  return _funneled_pwc(network, to, from, n, lop, laddr, rop, raddr);
}

/// Transform the get() operation into a parcel emulation.
static int
_funneled_get(void *network, void *to, hpx_addr_t from, size_t n,
              hpx_action_t lop, hpx_addr_t laddr) {
  // if there isn't a lop, then lsync should be HPX_NULL
  dbg_assert(lop || !laddr); // !lop => !laddr

  // go ahead an set the local lco if there is nothing to do
  if (!n) {
    hpx_call(laddr, lop, HPX_NULL, &here->rank, &laddr);
    return HPX_SUCCESS;
  }

  // Chain the lop handler to an LCO.
  hpx_addr_t lsync = HPX_NULL;
  if (lop) {
    lsync = hpx_lco_future_new(0);
    dbg_assert(lsync);
    int e = hpx_call_when_with_continuation(lsync, laddr, lop, lsync,
                                            hpx_lco_delete_action, &here->rank,
                                            &laddr);
    dbg_check(e, "failed to chain parcel\n");
  }

  // Concoct a global address that points to @p to @ here, and send it over.
  return hpx_call(from, isir_emulate_gwc, lsync, &n, &HPX_HERE, &to);
}

static hpx_parcel_t *
_funneled_probe(void *network, int nrx) {
  _funneled_t *isir = network;
  return sync_two_lock_queue_dequeue(&isir->recvs);
}

static void
_funneled_flush_all(void *network, int force) {
  _funneled_t *isir = network;
  if (isir->flush || force) {
    _send_all(isir);
    isend_buffer_flush(&isir->isends);
  }
}

static void
_funneled_set_flush(void *network) {
  _funneled_t *isir = network;
  sync_store(&isir->flush, 1, SYNC_RELEASE);
}

/// Create a network registration.
static void
_funneled_register_dma(void *obj, const void *base, size_t n, void *key) {
  _funneled_t *isir = obj;
  isir->xport->pin(base, n, key);
}

/// Release a network registration.
static void
_funneled_release_dma(void *obj, const void* base, size_t n) {
  _funneled_t *isir = obj;
  isir->xport->unpin(base, n);
}

static int
_funneled_progress(void *network) {
  _funneled_t *isir = network;
  hpx_parcel_t *chain = irecv_buffer_progress(&isir->irecvs);
  int n = 0;
  if (chain) {
    ++n;
    sync_two_lock_queue_enqueue(&isir->recvs, chain);
  }

  DEBUG_IF(n) {
    log_net("completed %d recvs\n", n);
  }

  int m = isend_buffer_progress(&isir->isends);

  DEBUG_IF(m) {
    log_net("completed %d sends\n", m);
  }

  _send_all(isir);

  return LIBHPX_OK;

  // suppress unused warnings
  (void)n;
  (void)m;
}

network_t *
network_isir_funneled_new(const config_t *cfg, struct boot *boot, gas_t *gas) {
  _funneled_t *network = malloc(sizeof(*network));
  if (!network) {
    log_error("could not allocate a funneled Isend/Irecv network\n");
    return NULL;
  }

  network->xport = isir_xport_new(cfg, gas);
  if (!network->xport) {
    log_error("could not initialize a transport.\n");
    free(network);
    return NULL;
  }

  network->vtable.type = HPX_NETWORK_ISIR;
  network->vtable.delete = _funneled_delete;
  network->vtable.progress = _funneled_progress;
  network->vtable.send = _funneled_send;
  network->vtable.command = _funneled_command;
  network->vtable.pwc = _funneled_pwc;
  network->vtable.put = _funneled_put;
  network->vtable.get = _funneled_get;
  network->vtable.probe = _funneled_probe;
  network->vtable.set_flush = _funneled_set_flush;
  network->vtable.flush_all = _funneled_flush_all;
  network->vtable.register_dma = _funneled_register_dma;
  network->vtable.release_dma = _funneled_release_dma;
  network->vtable.lco_get = isir_lco_get;
  network->vtable.lco_wait = isir_lco_wait;
  network->gas = gas;

  sync_store(&network->flush, 0, SYNC_RELEASE);
  sync_two_lock_queue_init(&network->sends, NULL);
  sync_two_lock_queue_init(&network->recvs, NULL);

  isend_buffer_init(&network->isends, network->xport, 64, cfg->isir_sendlimit,
            cfg->isir_testwindow);
  irecv_buffer_init(&network->irecvs, network->xport, 64, cfg->isir_recvlimit);

  return &network->vtable;
}
