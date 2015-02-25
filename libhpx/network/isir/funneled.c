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

#include <mpi.h>
#include <stdlib.h>
#include <hpx/builtins.h>
#include <libsync/queues.h>

#include "libhpx/debug.h"
#include "libhpx/gas.h"
#include "libhpx/libhpx.h"
#include "libhpx/network.h"
#include "libhpx/parcel.h"

#include "emulate_pwc.h"
#include "irecv_buffer.h"
#include "isend_buffer.h"
#include "isir.h"

#define _CAT1(S, T) S##T
#define _CAT(S, T) _CAT1(S, T)
#define _BYTES(S) (HPX_CACHELINE_SIZE - ((S) % HPX_CACHELINE_SIZE))
#define _PAD(S) const char _CAT(_padding,__LINE__)[_BYTES(S)]

typedef struct {
  network_t       vtable;
  gas_t             *gas;
  volatile int     flush;
  _PAD(sizeof(network_t) + sizeof(gas_t*) + sizeof(int));
  two_lock_queue_t sends;
  two_lock_queue_t recvs;
  isend_buffer_t  isends;
  irecv_buffer_t  irecvs;
} _funneled_t;

/// Transfer any parcels in the funneled sends queue into the isends buffer.
static void _send_all(_funneled_t *network) {
  hpx_parcel_t *p = NULL;
  while ((p = sync_two_lock_queue_dequeue(&network->sends))) {
    isend_buffer_append(&network->isends, p, HPX_NULL);
  }
}

/// Delete a funneled network.
static void _funneled_delete(network_t *network) {
  dbg_assert(network);

  _funneled_t *this = (void*)network;

  // flush sends if we're supposed to
  if (this->flush) {
    _send_all(this);
    isend_buffer_flush(&this->isends);
  }

  isend_buffer_fini(&this->isends);
  irecv_buffer_fini(&this->irecvs);

  hpx_parcel_t *p = NULL;
  while ((p = sync_two_lock_queue_dequeue(&this->sends))) {
    hpx_parcel_release(p);
  }
  while ((p = sync_two_lock_queue_dequeue(&this->recvs))) {
    hpx_parcel_release(p);
  }

  sync_two_lock_queue_fini(&this->sends);
  sync_two_lock_queue_fini(&this->recvs);

  free(this);
}

static int _funneled_send(network_t *network, hpx_parcel_t *p) {
  _funneled_t *this = (void*)network;
  sync_two_lock_queue_enqueue(&this->sends, p);
  return LIBHPX_OK;
}

static int _funneled_pwc(network_t *network,
                         hpx_addr_t to, const void *from, size_t n,
                         hpx_action_t lop, hpx_addr_t laddr,
                         hpx_action_t rop, hpx_addr_t raddr) {
  dbg_assert(lop || !laddr); // !lop => !lsync

  hpx_parcel_t *p = hpx_parcel_acquire(from, n);
  if (!p) {
    log_error("could not allocate a parcel to emulate put-with-completion\n");
    return LIBHPX_ENOMEM;
  }

  p->target = to;
  p->action = isir_emulate_pwc;
  p->c_action = rop;
  p->c_target = raddr;

  // if there's a local operation, then chain it through an lco that gets
  // triggered when the send finishes
  hpx_addr_t lsync = HPX_NULL;
  if (lop) {
    lsync = hpx_lco_future_new(0);
    dbg_assert(lsync);
    int e = hpx_call_when_with_continuation(lsync, laddr, lop, lsync,
                                            hpx_lco_delete_action,  &laddr);
    dbg_check(e, "failed to chain parcel\n");
  }
  return hpx_parcel_send(p, lsync);
}

static int _funneled_put(network_t *net,
                         hpx_addr_t to, const void *from, size_t n,
                         hpx_action_t lop, hpx_addr_t laddr) {
  return _funneled_pwc(net, to, from, n, lop, laddr, HPX_ACTION_NULL, HPX_NULL);
}

/// Transform the get() operation into a parcel emulation.
static int _funneled_get(network_t *network,
                         void *to, hpx_addr_t from, size_t n,
                         hpx_action_t lop, hpx_addr_t laddr)
{
  _funneled_t *isir = (_funneled_t*)network;

  // we only support future set externally
  if (lop && lop != hpx_lco_set_action) {
    log_error("Local completion other than hpx_lco_set_action not supported\n");
    return LIBHPX_EUNIMPLEMENTED;
  }

  // if there isn't a lop, then laddr should be HPX_NULL
  dbg_assert(lop || !laddr); // !lop => !laddr

  // go ahead an set the local lco if there is nothing to do
  if (!n) {
    hpx_lco_set(laddr, 0, NULL, HPX_NULL, HPX_NULL);
    return HPX_SUCCESS;
  }

  // make sure the to address is in the global address space
  if (!isir->gas->is_global(isir->gas, to)) {
    return log_error("network_get() expects a global heap address\n");
  }

  // and spawn the remote operation---hpx_call eagerly copies the args buffer so
  // there is no need to wait
  hpx_addr_t raddr = isir->gas->lva_to_gva(to);
  return hpx_xcall(from, isir_emulate_gwc, HPX_NULL, n, raddr, laddr);
}

static hpx_parcel_t *_funneled_probe(network_t *network, int nrx) {
  _funneled_t *this = (void*)network;
  return sync_two_lock_queue_dequeue(&this->recvs);
}

static void _funneled_set_flush(network_t *network) {
  _funneled_t *this = (void*)network;
  sync_store(&this->flush, 1, SYNC_RELEASE);
}

static int _funneled_progress(network_t *network) {
  _funneled_t *this = (void*)network;
  hpx_parcel_t *chain = irecv_buffer_progress(&this->irecvs);
  int n = 0;
  if (chain) {
    ++n;
    sync_two_lock_queue_enqueue(&this->recvs, chain);
  }

  DEBUG_IF(n) {
    log_net("completed %d recvs\n", n);
  }

  int m = isend_buffer_progress(&this->isends);

  DEBUG_IF(m) {
    log_net("completed %d sends\n", m);
  }

  _send_all(this);

  return LIBHPX_OK;

  // suppress unused warnings
  (void)n;
  (void)m;
}

network_t *network_isir_funneled_new(struct gas *gas, int nrx) {
  if (gas->type == HPX_GAS_SMP) {
    log_net("will not initialize a %s network for SMP\n",
            LIBHPX_NETWORK_TO_STRING[LIBHPX_NETWORK_ISIR]);
    return NULL;
  }

  _funneled_t *network = malloc(sizeof(*network));
  if (!network) {
    log_error("could not allocate a funneled Isend/Irecv network\n");
    return NULL;
  }

  network->vtable.type = LIBHPX_NETWORK_ISIR;
  network->vtable.delete = _funneled_delete;
  network->vtable.progress = _funneled_progress;
  network->vtable.send = _funneled_send;
  network->vtable.pwc = _funneled_pwc;
  network->vtable.put = _funneled_put;
  network->vtable.get = _funneled_get;
  network->vtable.probe = _funneled_probe;
  network->vtable.set_flush = _funneled_set_flush;

  network->gas = gas;

  sync_store(&network->flush, 0, SYNC_RELEASE);
  sync_two_lock_queue_init(&network->sends, NULL);
  sync_two_lock_queue_init(&network->recvs, NULL);
  isend_buffer_init(&network->isends, gas, 64, 0);
  irecv_buffer_init(&network->irecvs, 64, 0);

  return &network->vtable;
}
