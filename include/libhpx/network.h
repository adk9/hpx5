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
#ifndef LIBHPX_NETWORK_H
#define LIBHPX_NETWORK_H

/// @file include/libhpx/network.h
/// @brief Declare the network_class_t structure.
///
/// This file declares the interface to the parcel network subsystem in HPX. The
/// network's primary responsibility is to accept send requests from the
/// scheduler, and send them out via the configured transport.
#include "hpx/hpx.h"


/// All network objects implement the network_class interface.
typedef struct network_class network_class_t;


/// Create a new network.
///
/// This depends on the current boot and transport object to be configured in
/// the "here" locality.
network_class_t *network_new(void)
  HPX_MALLOC HPX_INTERNAL;


/// Delete a network object.
///
/// This does not synchronize. The caller is required to ensure that no other
/// threads may be operating on the network before making this call.
///
/// @param  network The network to delete.
void network_delete(network_class_t *network)
  HPX_NON_NULL(1) HPX_INTERNAL;


/// Shuts down the network.
///
/// Indicates that the network should shut down.
///
/// @param  network The network to shut down.
void network_shutdown(network_class_t *network)
  HPX_NON_NULL(1) HPX_INTERNAL;


/// A network barrier.
///
/// Should only be called by one thread per locality. For a full system barrier,
/// callers should use the scheduler barrier first, and the thread that arrives
/// last at the scheduler barrier (see sync barrier interface) can call the
/// network barrier.
///
/// To block all threads through the network barrier, the user can re-join the
/// scheduler barrier with all of the other threads, thus making a full global
/// barrier a scheduler->network->scheduler barrier.
///
/// @param  network The network which implements the barrier.
void network_barrier(network_class_t *network)
  HPX_NON_NULL(1) HPX_INTERNAL;


/// Initiate a parcel send over the network.
///
/// This is an asynchronous interface, the parcel will be sent at some point in
/// the future, and is guaranteed to be delivered. Delivery order is not
/// guaranteed. This transfers ownership of the parcel @p p, the client should
/// not access the sent parcel again---in particular, the client should not
/// release the parcel.
///
/// This call will block if there are not enough resources available to satisfy
/// the enqueue. It may progress the network as well, though that behavior is
/// not guaranteed.
///
/// @todo There is currently no way to test for send completion. We should add a
///       future parameter so that the sender can wait if necessary.
///
/// @param  network The network to use for the send.
/// @param        p The parcel to send.
void network_tx_enqueue(network_class_t *network, hpx_parcel_t *p)
  HPX_NON_NULL(1, 2) HPX_INTERNAL;


/// Complete a parcel send over the network.
hpx_parcel_t *network_tx_dequeue(network_class_t *network)
  HPX_NON_NULL(1) HPX_INTERNAL;


void network_rx_enqueue(network_class_t *network, hpx_parcel_t *p)
  HPX_NON_NULL(1, 2) HPX_INTERNAL;


hpx_parcel_t *network_rx_dequeue(network_class_t *network)
  HPX_NON_NULL(1) HPX_INTERNAL;


struct routing;
struct routing *network_get_routing(network_class_t *network)
  HPX_NON_NULL(1) HPX_INTERNAL;


void network_set_shutdown_src(network_class_t *network, uint32_t src)
  HPX_NON_NULL(1) HPX_INTERNAL;


uint32_t network_get_shutdown_src(network_class_t *network)
  HPX_NON_NULL(1) HPX_INTERNAL;

#endif // LIBHPX_NETWORK_H
