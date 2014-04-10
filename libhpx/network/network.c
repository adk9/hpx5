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
#include "libhpx/debug.h"
#include "libhpx/network.h"
#include "libhpx/parcel.h"
#include "libhpx/system.h"
#include "libhpx/transport.h"

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

static ms_queue_t *network_tx_port = NULL;
static ms_queue_t *network_rx_port = NULL;

/// ----------------------------------------------------------------------------
/// The network class data.
/// ----------------------------------------------------------------------------
struct network {
  const boot_t       *boot;                     // rank, n_ranks
  transport_t   *transport;                     // byte-based send/recv

  ms_queue_t         sends;                     // half duplex port for send
  ms_queue_t         recvs;                     // half duplex port for recv

  SYNC_ATOMIC(_network_state_t) state;          // state for progress
};


void network_tx_enqueue(hpx_parcel_t *p) {
  sync_ms_queue_enqueue(network_tx_port, p);
}

hpx_parcel_t *network_tx_dequeue(void) {
  return (hpx_parcel_t*)sync_ms_queue_dequeue(network_tx_port);
}

void network_rx_enqueue(hpx_parcel_t *p) {
  sync_ms_queue_enqueue(network_rx_port, p);
}

hpx_parcel_t *network_rx_dequeue(void) {
  return (hpx_parcel_t*)sync_ms_queue_dequeue(network_rx_port);
}


/// Allocate a new network. The network currently consists of a single, shared
/// Tx/Rx port---implemented as two M&S queues, two lists of pending transport
/// requests, and a freelist for request. There's also a shutdown flag that is
/// set asynchronously and tested in the progress loop.
network_t *network_new(const boot_t *boot, transport_t *transport) {
  network_t *n = malloc(sizeof(*n));
  n->boot         = boot;
  n->transport    = transport;

  network_tx_port = &n->sends;
  sync_ms_queue_init(network_tx_port);

  network_rx_port = &n->recvs;
  sync_ms_queue_init(network_rx_port);

  sync_store(&n->state, _STATE_RUNNING, SYNC_RELEASE);
  return n;
}

/// ----------------------------------------------------------------------------
/// Shuts down the network.
///
/// If network_shutdown() is called directly, the continuation
/// address is set to HPX_NULL. In that case, we simply skip the
/// parcel creation and shutdown the network progress thread.
/// ----------------------------------------------------------------------------
void network_shutdown(network_t *network) {
  // shutdown the network progress thread.
  sync_cas(&network->state, _STATE_RUNNING, _STATE_SHUTDOWN_PENDING, SYNC_RELEASE,
           SYNC_RELAXED);
}

void network_delete(network_t *network) {
  if (!network)
    return;

  hpx_parcel_t *p = NULL;

  while ((p = sync_ms_queue_dequeue(&network->sends)))
    hpx_parcel_release(p);
  sync_ms_queue_fini(&network->sends);
  network_tx_port = NULL;

  while ((p = sync_ms_queue_dequeue(&network->recvs)))
    hpx_parcel_release(p);
  sync_ms_queue_fini(&network->recvs);
  network_rx_port = NULL;

  free(network);
}


void network_send(network_t *network, hpx_parcel_t *p) {
  // check loopback via rank, on loopback push into the recv queue
  hpx_addr_t target = hpx_parcel_get_target(p);
  if (hpx_addr_try_pin(target, NULL))
    network_rx_enqueue(p);
  else
    network_tx_enqueue(p);
}


hpx_parcel_t *network_recv(network_t *network) {
  return network_rx_dequeue();
}


int network_progress(network_t *network) {
  _network_state_t state; sync_load(state, &network->state, SYNC_ACQUIRE);
  if (state != _STATE_RUNNING) {
    sync_cas(&network->state, _STATE_SHUTDOWN_PENDING, _STATE_SHUTDOWN,
             SYNC_RELEASE, SYNC_RELAXED);
    return state;
  }

  transport_progress(network->transport, false);
  return state;
}


void network_barrier(network_t *network) {
  transport_barrier(network->transport);
}
