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

/// ----------------------------------------------------------------------------
/// @file network.h
///
/// This file defines the interface to the network subsystem in HPX. The
/// network's primary responsibility is to accept send requests from the
/// scheduler, and send them out via the configured transport.
/// ----------------------------------------------------------------------------
#include "hpx/hpx.h"

struct routing;

typedef struct network_class network_class_t;


/// ----------------------------------------------------------------------------
/// Create a new network.
///
/// @param b - the boot object (contains rank, nranks, etc)
/// @param t - the byte transport object to
/// ----------------------------------------------------------------------------
HPX_INTERNAL network_class_t *network_new(void)
  HPX_MALLOC;


/// ----------------------------------------------------------------------------
/// Delete the network object.
///
/// This does not synchronize. The caller is required to ensure that no other
/// threads may be operating on the network before making this call.
///
/// @param network - the network to delete
/// ----------------------------------------------------------------------------
HPX_INTERNAL void network_delete(network_class_t *network)
  HPX_NON_NULL(1);

/// ----------------------------------------------------------------------------
/// Shuts down the network.
///
/// This simply shuts down the local network progress thread, if there
/// is one.
/// ----------------------------------------------------------------------------
HPX_INTERNAL void network_shutdown(network_class_t *network)
  HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// Send a parcel using the network.
///
/// This may loopback in the network, so it is safe to call for every
/// parcel. On loopback, the parcel will become available through
/// network_recv(). This can act as a poor man's load balancing scheme.
///
/// @param network - the network object to send through
/// @param  parcel - the parcel to send
/// ----------------------------------------------------------------------------
HPX_INTERNAL void network_send(network_class_t *network, hpx_parcel_t *parcel)
  HPX_NON_NULL(1, 2);


/// ----------------------------------------------------------------------------
/// Receive a parcel from the network.
///
/// @param network - the network to receive from
/// @returns       - NULL, or a parcel received from the network
/// ----------------------------------------------------------------------------
HPX_INTERNAL hpx_parcel_t *network_recv(network_class_t *network)
  HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// A network barrier.
///
/// Should only be called by one thread per locality. For a full system barrier,
/// callers should use the scheduler barrier first.
///
/// @param  status - the status future that tracks completion
/// ----------------------------------------------------------------------------
HPX_INTERNAL void network_barrier(network_class_t *network)
  HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// The main network operation.
///
/// As usual, the network_progress() function "does stuff" in the network layer
/// to make sure that everything else works correctly. This needs to be called
/// periodically, or nothing will happen in the network.
///
/// NB: OTHER NETWORK OPERATIONS DO NOT CALL THIS FUNCTION.
///
/// @param network - the network to manage
/// @returns       - non-0 to indicate that the network has been shutdown.
/// ----------------------------------------------------------------------------
HPX_INTERNAL int network_progress(network_class_t *network)
  HPX_NON_NULL(1);

///
HPX_INTERNAL void network_tx_enqueue(hpx_parcel_t *p) HPX_NON_NULL(1);

///
HPX_INTERNAL hpx_parcel_t *network_tx_dequeue(void);

///
HPX_INTERNAL void network_rx_enqueue(hpx_parcel_t *p) HPX_NON_NULL(1);

///
HPX_INTERNAL hpx_parcel_t *network_rx_dequeue(void);

///
HPX_INTERNAL struct routing *network_get_routing(network_class_t *network);

#endif // LIBHPX_NETWORK_H
