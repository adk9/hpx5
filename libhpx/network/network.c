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

/// @file libhpx/network/network.c
/// @brief Manages the HPX network.
///
/// This file deals with the complexities of the HPX network interface,
/// shielding it from the details of the underlying transport interface.
#include <assert.h>
#include <stdlib.h>

#include <libsync/sync.h>
#include <libsync/queues.h>
#include <libsync/spscq.h>
#include <libsync/locks.h>

#include <libhpx/boot.h>
#include <libhpx/debug.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/network.h>
#include <libhpx/parcel.h>
#include <libhpx/scheduler.h>
#include <libhpx/stats.h>
#include <libhpx/system.h>
#include <libhpx/transport.h>
#include <libhpx/routing.h>

#include "isir/isir.h"

//#define _QUEUE(pre, post) pre##spscq##post
//#define _QUEUE(pre, post) pre##ms_queue##post
#define _QUEUE(pre, post) pre##two_lock_queue##post
#define _QUEUE_T _QUEUE(, _t)
#define _QUEUE_INIT _QUEUE(sync_, _init)
#define _QUEUE_FINI _QUEUE(sync_, _fini)
#define _QUEUE_ENQUEUE _QUEUE(sync_, _enqueue)
#define _QUEUE_DEQUEUE _QUEUE(sync_, _dequeue)
#define _QUEUE_NODE _QUEUE(,_node_t)


/// The network class data.
struct _network {
  network_t                  vtable;
  volatile int                flush;
  struct transport_class *transport;
  int                           nrx;

  // make sure the rest of this structure is cacheline aligned
  const char _paddinga[HPX_CACHELINE_SIZE - ((sizeof(network_t) +
                                              sizeof(int) +
                                              sizeof(struct transport_class*) +
                                              sizeof(int)) %
                                             HPX_CACHELINE_SIZE)];

  _QUEUE_T                 tx;                  // half duplex port for send
  _QUEUE_T                 rx;
};


static void _finish(struct _network *network) {
  int flush = sync_load(&network->flush, SYNC_ACQUIRE);
  if (flush) {
    transport_progress(network->transport, TRANSPORT_FLUSH);
  }
  else {
    transport_progress(network->transport, TRANSPORT_CANCEL);
  }
}


static void _delete(network_t *o) {
  if (!o)
    return;

  struct _network *network = (struct _network*)o;

  _finish(network);

  hpx_parcel_t *p = NULL;

  while ((p = _QUEUE_DEQUEUE(&network->tx))) {
    hpx_parcel_release(p);
  }
  _QUEUE_FINI(&network->tx);

  free(network);
}


static void _barrier(network_t *o) {
  struct _network *network = (struct _network*)o;
  transport_barrier(network->transport);
}


static int _send(network_t *o, hpx_parcel_t *p, hpx_addr_t complete) {
  struct _network *network = (struct _network*)o;
  _QUEUE_ENQUEUE(&network->tx, p);
  return LIBHPX_OK;
}


hpx_parcel_t *network_tx_dequeue(network_t *o) {
  struct _network *network = (struct _network*)o;
  return _QUEUE_DEQUEUE(&network->tx);
}


static hpx_parcel_t *_probe_parcel(network_t *o, int nrx) {
  struct _network *network = (struct _network*)o;
  return _QUEUE_DEQUEUE(&network->rx);
}


static void _set_flush(network_t *o) {
  struct _network *network = (struct _network*)o;
  sync_store(&network->flush, 1, SYNC_RELEASE);
}


int network_try_notify_rx(network_t *o, hpx_parcel_t *p) {
  struct _network *network = (struct _network*)o;
  _QUEUE_ENQUEUE(&network->rx, p);
  return 1;
}


static int _progress(network_t *n) {
  struct _network *network = (void*)n;
  transport_progress(network->transport, TRANSPORT_POLL);
  return LIBHPX_OK;
}

static network_t *_old_new(int nrx) {
  struct _network *n = NULL;
  int e = posix_memalign((void**)&n, HPX_CACHELINE_SIZE, sizeof(*n));
  if (e) {
    dbg_error("failed to allocate a network.\n");
    return NULL;
  }

  n->vtable.delete = _delete;
  n->vtable.progress = _progress;
  n->vtable.barrier = _barrier;
  n->vtable.send = _send;
  n->vtable.probe = _probe_parcel;
  n->vtable.set_flush = _set_flush;

  n->transport = here->transport;
  sync_store(&n->flush, 0, SYNC_RELEASE);
  n->nrx = nrx;

  assert(n->nrx < 128);

  _QUEUE_INIT(&n->tx, 0);
  _QUEUE_INIT(&n->rx, 0);

  return &n->vtable;
}

network_t *network_new(libhpx_network_t type, struct gas_class *gas, int nrx) {
  // return _old_new(nrx);
  return network_isir_funneled_new(gas, nrx);
}
