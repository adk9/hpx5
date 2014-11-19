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

#include <libsync/sync.h>
#include <libsync/queues.h>

#include "libhpx/boot.h"
#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/network.h"
#include "libhpx/parcel.h"
#include "libhpx/system.h"
#include "libhpx/transport.h"
#include "libhpx/routing.h"


#ifdef ENABLE_TAU
#define TAU_DEFAULT 1
#include <TAU.h>
#endif

#define _QUEUE(pre, post) pre##two_lock_queue##post
//#define _QUEUE(pre, post) pre##ms_queue##post
#define _QUEUE_T _QUEUE(, _t)
#define _QUEUE_INIT _QUEUE(sync_, _init)
#define _QUEUE_FINI _QUEUE(sync_, _fini)
#define _QUEUE_ENQUEUE _QUEUE(sync_, _enqueue_node)
#define _QUEUE_DEQUEUE _QUEUE(sync_, _dequeue_node)
#define _QUEUE_NODE _QUEUE(,_node_t)

/// The network class data.
struct network_class {
  _QUEUE_T                         tx;          // half duplex port for send
  _QUEUE_T                         rx;          // half duplex port
  routing_t                  *routing;          // for adaptive routing
  int                           flush;
};


void network_tx_enqueue(network_class_t *network, hpx_parcel_t *p) {
  assert(p);
  _QUEUE_NODE *node = malloc(sizeof(*node));
  assert(node);
  node->value = p;
  node->next = NULL;
  _QUEUE_ENQUEUE(&network->tx, node);
}


hpx_parcel_t *network_tx_dequeue(network_class_t *network) {
  hpx_parcel_t *p  = NULL;
  _QUEUE_NODE *node = _QUEUE_DEQUEUE(&network->tx);
  if (node) {
    p = node->value;
    assert(p);
    free(node);
  }
  return p;
}

void network_rx_enqueue(network_class_t *network, hpx_parcel_t *p) {
  assert(p);
  _QUEUE_NODE *node = malloc(sizeof(*node));
  assert(node);
  node->value = p;
  node->next = NULL;
  _QUEUE_ENQUEUE(&network->rx, node);
}

hpx_parcel_t *network_rx_dequeue(network_class_t *network) {
  hpx_parcel_t *p  = NULL;
  _QUEUE_NODE *node = _QUEUE_DEQUEUE(&network->rx);
  if (node) {
    p = node->value;
    assert(p);
    free(node);
  }
  return p;
}


/// Allocate a new network. The network currently consists of a single, shared
/// Tx/Rx port---implemented as two M&S queues, two lists of pending transport
/// requests, and a freelist for request. There's also a shutdown flag that is
/// set asynchronously and tested in the progress loop.
network_class_t *network_new(void) {

#ifdef ENABLE_TAU
          TAU_START("network_new");
#endif

  network_class_t *n = malloc(sizeof(*n));
  if (!n) {
    dbg_error("network: failed to allocate a network.\n");
    return NULL;
  }

  _QUEUE_INIT(&n->tx, NULL);
  _QUEUE_INIT(&n->rx, NULL);

  n->routing = routing_new();
  if (!n->routing) {
    dbg_error("network: failed to start routing update manager.\n");
    free(n);
    return NULL;
  }

  n->flush = 0;
  dbg_log("initialized parcel network.\n");

#ifdef ENABLE_TAU
          TAU_STOP("network_new");
#endif

  return n;
}


void network_delete(network_class_t *network) {
#ifdef ENABLE_TAU
          TAU_START("network_delete");
#endif
  if (!network)
    return;

  hpx_parcel_t *p = NULL;

  while ((p = network_tx_dequeue(network))) {
    hpx_parcel_release(p);
  }
  _QUEUE_FINI(&network->tx);

  while ((p = network_rx_dequeue(network))) {
    hpx_parcel_release(p);
  }
  _QUEUE_FINI(&network->rx);

  if (network->routing) {
    routing_delete(network->routing);
  }

  free(network);
#ifdef ENABLE_TAU
          TAU_STOP("network_delete");
#endif
}


void network_shutdown(network_class_t *network) {
  if (network->flush)
    transport_progress(here->transport, TRANSPORT_FLUSH);
  else
    transport_progress(here->transport, TRANSPORT_CANCEL);
}


void network_barrier(network_class_t *network) {
#ifdef ENABLE_TAU
          TAU_START("network_barrier");
#endif
  transport_barrier(here->transport);
#ifdef ENABLE_TAU
          TAU_STOP("network_barrier");
#endif
}


routing_t *network_get_routing(network_class_t *network) {
  return network->routing;
}

void network_flush_on_shutdown(network_class_t *network) {
  network->flush = 1;
}
