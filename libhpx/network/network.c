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
/// @file libhpx/network/network.c
/// @brief Manages the HPX network.
///
/// This file deals with the complexities of the HPX network interface,
/// shielding it from the details of the underlying transport interface.
/// ----------------------------------------------------------------------------
#include <assert.h>
#include <stdlib.h>

#include "libsync/queues.h"

#include "libhpx/boot.h"
#include "libhpx/btt.h"
#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/network.h"
#include "libhpx/parcel.h"
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


/// ----------------------------------------------------------------------------
/// The network class data.
/// ----------------------------------------------------------------------------
struct network_class {
  _QUEUE_T                         tx;          // half duplex port for send
  _QUEUE_T                         rx;          // half duplex port for recv
  routing_class_t            *routing;          // for adaptive routing
};


void network_tx_enqueue(network_class_t *network, hpx_parcel_t *p) {
  _QUEUE_ENQUEUE(&network->tx, p);
}

hpx_parcel_t *network_tx_dequeue(network_class_t *network) {
  return (hpx_parcel_t*)_QUEUE_DEQUEUE(&network->tx);
}

void network_rx_enqueue(network_class_t *network, hpx_parcel_t *p) {
  _QUEUE_ENQUEUE(&network->rx, p);
}

hpx_parcel_t *network_rx_dequeue(network_class_t *network) {
  return (hpx_parcel_t*)_QUEUE_DEQUEUE(&network->rx);
}


/// Allocate a new network. The network currently consists of a single, shared
/// Tx/Rx port---implemented as two M&S queues, two lists of pending transport
/// requests, and a freelist for request. There's also a shutdown flag that is
/// set asynchronously and tested in the progress loop.
network_class_t *network_new(void) {
  network_class_t *n = malloc(sizeof(*n));
  if (!n) {
    dbg_error("failed to allocate a network.\n");
    return NULL;
  }

  _QUEUE_INIT(&n->tx, NULL);
  _QUEUE_INIT(&n->rx, NULL);

  n->routing = routing_new(HPX_ROUTING_DEFAULT);
  if (!n->routing) {
    dbg_error("failed to start routing update manager.\n");
    free(n);
    return NULL;
  }
  return n;
}


void network_delete(network_class_t *network) {
  if (!network)
    return;

  hpx_parcel_t *p = NULL;

  while ((p = _QUEUE_DEQUEUE(&network->tx)))
    hpx_parcel_release(p);
  _QUEUE_FINI(&network->tx);

  while ((p =_QUEUE_DEQUEUE(&network->rx)))
    hpx_parcel_release(p);
  _QUEUE_FINI(&network->rx);

  if (network->routing)
    routing_delete(network->routing);

  free(network);
}


void network_barrier(network_class_t *network) {
  transport_barrier(here->transport);
}


routing_class_t *network_get_routing(network_class_t *network) {
  return network->routing;
}


void *network_malloc(size_t bytes, size_t align) {
  return transport_malloc(here->transport, bytes, align);
}


void network_free(void *p) {
  transport_free(here->transport, p);
}
