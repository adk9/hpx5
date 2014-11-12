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
#include <pthread.h>

#include <libsync/sync.h>
#include <libsync/queues.h>

#include "libhpx/boot.h"
#include "libhpx/debug.h"
#include "libhpx/libhpx.h"
#include "libhpx/locality.h"
#include "libhpx/network.h"
#include "libhpx/parcel.h"
#include "libhpx/stats.h"
#include "libhpx/system.h"
#include "libhpx/transport.h"
#include "libhpx/routing.h"


#define _QUEUE(pre, post) pre##two_lock_queue##post
//#define _QUEUE(pre, post) pre##ms_queue##post
#define _QUEUE_T _QUEUE(, _t)
#define _QUEUE_INIT _QUEUE(sync_, _init)
#define _QUEUE_FINI _QUEUE(sync_, _fini)
#define _QUEUE_ENQUEUE _QUEUE(sync_, _enqueue)
#define _QUEUE_DEQUEUE _QUEUE(sync_, _dequeue)
#define _QUEUE_NODE _QUEUE(,_node_t)


/// The network class data.
struct _network {
  struct network       vtable;
  const char          padding[HPX_CACHELINE_SIZE - sizeof(struct network)];
  _QUEUE_T                 tx;                  // half duplex port for send
  _QUEUE_T                 rx;                  // half duplex port
  struct transport_class *transport;
  pthread_t          progress;
  volatile int          flush;
};


static void *_progress(void *o) {
  struct _network *network = o;

  // we have to join the GAS so that we can allocate parcels in here.
  int e = here->gas->join();
  if (e) {
    dbg_error("network failed to join the global address space.\n");
  }

  while (true) {
    pthread_testcancel();
    profile_ctr(thread_get_stats()->progress++);
    transport_progress(network->transport, TRANSPORT_POLL);
    pthread_yield();
  }
  return NULL;
}


static void _delete(struct network *o) {
  if (!o)
    return;

  struct _network *network = (struct _network*)o;

  hpx_parcel_t *p = NULL;

  while ((p = _QUEUE_DEQUEUE(&network->tx))) {
    hpx_parcel_release(p);
  }
  _QUEUE_FINI(&network->tx);

  while ((p = _QUEUE_DEQUEUE(&network->rx))) {
    hpx_parcel_release(p);
  }
  _QUEUE_FINI(&network->rx);

  free(network);
}


static int _startup(struct network *o) {
  struct _network *network = (struct _network*)o;
  int e = pthread_create(&network->progress, NULL, _progress, network);
  if (e) {
    dbg_error("failed to start network progress.\n");
    return LIBHPX_ERROR;
  }
  else {
    dbg_log("started network progress.\n");
    return LIBHPX_OK;
  }
}


static void _shutdown(struct network *o) {
  struct _network *network = (struct _network*)o;
  int e = pthread_cancel(network->progress);
  if (e) {
    dbg_error("could not cancel the network progress thread.\n");
  }

  e = pthread_join(network->progress, NULL);
  if (e) {
    dbg_error("could not join the network progress thread.\n");
  }
  else {
    dbg_log("shutdown network progress.\n");
  }

  int flush = sync_load(&network->flush, SYNC_ACQUIRE);
  if (flush) {
    transport_progress(network->transport, TRANSPORT_FLUSH);
  }
  else {
    transport_progress(network->transport, TRANSPORT_CANCEL);
  }
}


static void _barrier(struct network *o) {
  struct _network *network = (struct _network*)o;
  transport_barrier(network->transport);
}


void network_tx_enqueue(struct network *o, hpx_parcel_t *p) {
  struct _network *network = (struct _network*)o;
  _QUEUE_ENQUEUE(&network->tx, p);
}


hpx_parcel_t *network_tx_dequeue(struct network *o) {
  struct _network *network = (struct _network*)o;
  return _QUEUE_DEQUEUE(&network->tx);
}


void network_rx_enqueue(struct network *o, hpx_parcel_t *p) {
  struct _network *network = (struct _network*)o;
  _QUEUE_ENQUEUE(&network->rx, p);
}


hpx_parcel_t *network_rx_dequeue(struct network *o) {
  struct _network *network = (struct _network*)o;
  return _QUEUE_DEQUEUE(&network->rx);
}

void network_flush_on_shutdown(struct network *o) {
  struct _network *network = (struct _network*)o;
  sync_store(&network->flush, 1, SYNC_RELEASE);
}

struct network *network_new(void) {
  struct _network *n = malloc(sizeof(*n));
  if (!n) {
    dbg_error("failed to allocate a network.\n");
    return NULL;
  }

  n->vtable.delete = _delete;
  n->vtable.startup = _startup;
  n->vtable.shutdown = _shutdown;
  n->vtable.barrier = _barrier;

  _QUEUE_INIT(&n->tx, NULL);
  _QUEUE_INIT(&n->rx, NULL);
  n->transport = here->transport;
  sync_store(&n->flush, 0, SYNC_RELEASE);

  return &n->vtable;
}
