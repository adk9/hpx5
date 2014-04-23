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
#include "libsync/sync.h"

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

typedef enum {
  _STATE_RUNNING = 0,
  _STATE_SHUTDOWN_PENDING,
  _STATE_SHUTDOWN,
  _STATE_MAX
} _network_state_t;

// The Tx/Rx ports need to be accessed by the transport layer for
// parcel communication. As such, this creates a circular dependency
// between the network and transport objects. The network object
// maintains global references to these ports and exports functions to
// manipulate them.

static _QUEUE_T *network_tx_port = NULL;
static _QUEUE_T *network_rx_port = NULL;

/// ----------------------------------------------------------------------------
/// The network class data.
/// ----------------------------------------------------------------------------
struct network_class {
  _QUEUE_T         sends;                     // half duplex port for send
  _QUEUE_T         recvs;                     // half duplex port for recv

  SYNC_ATOMIC(_network_state_t) state;          // state for progress

  routing_t         *routing;                   // for adaptive routing
};


void network_tx_enqueue(hpx_parcel_t *p) {
  _QUEUE_ENQUEUE(network_tx_port, p);
}

hpx_parcel_t *network_tx_dequeue(void) {
  return (hpx_parcel_t*)_QUEUE_DEQUEUE(network_tx_port);
}

void network_rx_enqueue(hpx_parcel_t *p) {
  _QUEUE_ENQUEUE(network_rx_port, p);
}

hpx_parcel_t *network_rx_dequeue(void) {
  return (hpx_parcel_t*)_QUEUE_DEQUEUE(network_rx_port);
}


/// Allocate a new network. The network currently consists of a single, shared
/// Tx/Rx port---implemented as two M&S queues, two lists of pending transport
/// requests, and a freelist for request. There's also a shutdown flag that is
/// set asynchronously and tested in the progress loop.
network_class_t *network_new(void) {
  network_class_t *n = malloc(sizeof(*n));

  assert(!network_tx_port);
  assert(!network_rx_port);

  network_tx_port = &n->sends;
  network_rx_port = &n->recvs;

  _QUEUE_INIT(network_tx_port, NULL);
  _QUEUE_INIT(network_rx_port, NULL);

  sync_store(&n->state, _STATE_RUNNING, SYNC_RELEASE);

  n->routing = routing_new();
  if (!n->routing) {
    dbg_error("failed to start routing update manager.\n");
    free(n);
    return NULL;
  }
  return n;
}


/// ----------------------------------------------------------------------------
/// Shuts down the network.
///
/// If network_shutdown() is called directly, the continuation
/// address is set to HPX_NULL. In that case, we simply skip the
/// parcel creation and shutdown the network progress thread.
/// ----------------------------------------------------------------------------
void network_shutdown(network_class_t *network) {
  // shutdown the network progress thread.
  _network_state_t running = _STATE_RUNNING; // atomic.h workaround
  sync_cas(&network->state, running, _STATE_SHUTDOWN_PENDING, SYNC_RELEASE,
           SYNC_RELAXED);
}


void network_delete(network_class_t *network) {
  if (!network)
    return;

  hpx_parcel_t *p = NULL;

  while ((p = _QUEUE_DEQUEUE(&network->sends)))
    hpx_parcel_release(p);
  _QUEUE_FINI(&network->sends);
  network_tx_port = NULL;

  while ((p =_QUEUE_DEQUEUE(&network->recvs)))
    hpx_parcel_release(p);
  _QUEUE_FINI(&network->recvs);
  network_rx_port = NULL;

  if (network->routing)
    routing_delete(network->routing);

  free(network);
}


void network_send(network_class_t *network, hpx_parcel_t *p) {
  // check loopback via rank, on loopback push into the recv queue
  hpx_addr_t target = hpx_parcel_get_target(p);
  uint32_t owner = btt_owner(here->btt, target);
  if (owner == here->rank)
    network_rx_enqueue(p);
  else
    network_tx_enqueue(p);
}


hpx_parcel_t *network_recv(network_class_t *network) {
  return network_rx_dequeue();
}


int network_progress(network_class_t *network) {
  _network_state_t state;
  sync_load(state, &network->state, SYNC_ACQUIRE);
  if (state != _STATE_RUNNING) {
    state = _STATE_SHUTDOWN_PENDING; // atomic.h workaround
    sync_cas(&network->state, state, _STATE_SHUTDOWN, SYNC_RELEASE,
        SYNC_RELAXED);

    // flush out the pending parcels
    transport_progress(here->transport, true);
    return state;
  }

  transport_progress(here->transport, false);
  return state;
}


void network_barrier(network_class_t *network) {
  transport_barrier(here->transport);
}

routing_t *network_get_routing(network_class_t *network) {
  return network->routing;
}
