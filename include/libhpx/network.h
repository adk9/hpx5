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


struct boot;
struct transport;
typedef struct network network_t;


/// ----------------------------------------------------------------------------
/// Create a new network.
///
/// @param b - the boot object (contains rank, nranks, etc)
/// @param t - the byte transport object to
/// ----------------------------------------------------------------------------
HPX_INTERNAL network_t *network_new(const struct boot *b, struct transport *t)
  HPX_NON_NULL(1, 2);


/// ----------------------------------------------------------------------------
/// Delete the network object.
///
/// This does not synchronize. The caller is required to ensure that no other
/// threads may be operating on the network before making this call.
///
/// @param network - the network to delete
/// ----------------------------------------------------------------------------
HPX_INTERNAL void network_delete(network_t *network)
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
HPX_INTERNAL void network_send(network_t *network, hpx_parcel_t *parcel)
  HPX_NON_NULL(1, 2);


/// ----------------------------------------------------------------------------
/// Receive a parcel from the network.
///
/// @param network - the network to receive from
/// @returns       - NULL, or a parcel received from the network
/// ----------------------------------------------------------------------------
HPX_INTERNAL hpx_parcel_t *network_recv(network_t *network)
  HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// A network barrier.
///
/// Should only be called by one thread per locality. For a full system barrier,
/// callers should use the scheduler barrier first.
/// ----------------------------------------------------------------------------
HPX_INTERNAL void network_barrier(network_t *network)
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
/// ----------------------------------------------------------------------------
HPX_INTERNAL void network_progress(network_t *network)
  HPX_NON_NULL(1);


#endif // LIBHPX_NETWORK_H
