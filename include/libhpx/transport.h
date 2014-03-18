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
#ifndef LIBHPX_TRANSPORT_H
#define LIBHPX_TRANSPORT_H

#include <stddef.h>
#include "attributes.h"

struct boot;
typedef struct transport transport_t;


#define TRANSPORT_ANY_SOURCE -1


/// ----------------------------------------------------------------------------
/// Allocate and initialize a network.
///
/// This is a factory method that actually picks concrete networks based on what
/// is currently available.
/// ----------------------------------------------------------------------------
HPX_INTERNAL transport_t *transport_new(const struct boot *boot)
  HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// Delete the transport.
/// ----------------------------------------------------------------------------
HPX_INTERNAL void transport_delete(transport_t*)
  HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// Determine how much space needs to be allocated for a request.
/// ----------------------------------------------------------------------------
HPX_INTERNAL int transport_request_size(const transport_t *t)
  HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// Pin a block of memory
/// ----------------------------------------------------------------------------
HPX_INTERNAL void transport_pin(transport_t *t, const void *buffer, size_t len)
  HPX_NON_NULL(1, 2);


/// ----------------------------------------------------------------------------
/// Unpin a block of memory
/// ----------------------------------------------------------------------------
HPX_INTERNAL void transport_unpin(transport_t *t, const void *buffer,
                                  size_t len)
  HPX_NON_NULL(1, 2);


/// ----------------------------------------------------------------------------
/// Send a buffer to a destination.
///
/// The buffer must not be modified between this operation and successful
/// completion of the request.
///
/// @param       t - the transport object
/// @param    dest - the destination rank
/// @param  buffer - the buffer to send
/// @param    size - the number of bytes in the buffer
/// @param request - a transport-specific object that can be used to reference
///                  this send operation later
/// @returns       - HPX_SUCCESS or HPX_ERROR
/// ----------------------------------------------------------------------------
HPX_INTERNAL int transport_send(transport_t *t, int dest, const void *buffer,
                                size_t size, void *request)
  HPX_NON_NULL(1, 3);


/// ----------------------------------------------------------------------------
/// Polls to see if any bytes are ready for us.
///
/// @param           p - the transport
/// @param[in/out] src - the source we'd like to probe (TRANSPORT_ANY_SOURCE
///                      normally), returns the source to recv from, if anything
///                      is ready
/// @returns           - the number of ready bytes to receive from src
/// ----------------------------------------------------------------------------
HPX_INTERNAL size_t transport_probe(transport_t *t, int *src)
  HPX_NON_NULL(1, 2);


/// ----------------------------------------------------------------------------
/// Receive parcel data from a source.
///
/// It is expected that the @p src and @p size of the receive values are the
/// result of a successful transport_probe() operation.
///
/// @param       t - the transport object
/// @param     src - the source rank
/// @param  buffer - the target buffer
/// @param    size - the number of bytes available
/// @param request - an appropriately sized request to poll completion with
/// @returns       - HPX_SUCCESS or HPX_ERROR
/// ----------------------------------------------------------------------------
HPX_INTERNAL int transport_recv(transport_t *t, int src, void *buffer,
                                size_t size, void *request)
  HPX_NON_NULL(1, 3);


/// ----------------------------------------------------------------------------
/// Test to see if a send/recv operation is complete.
///
/// @param        t - the transport
/// @param  request - the request identifier
/// @param complete - 0 if still active, non-0 otherwise
/// @returns        - HPX_SUCCESS or HPX_ERROR
/// ----------------------------------------------------------------------------
HPX_INTERNAL int transport_test_sendrecv(transport_t *t, const void *request,
                                         int *complete)
  HPX_NON_NULL(1, 2);


/// ----------------------------------------------------------------------------
/// A low-level transport barrier.
///
/// @param t - the transport
/// ----------------------------------------------------------------------------
HPX_INTERNAL void transport_barrier(transport_t *t)
  HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// Give the transport an opportunity to adjust a size to something that it can
/// send.
///
/// This is used during parcel allocation.
/// ----------------------------------------------------------------------------
HPX_INTERNAL int transport_adjust_size(transport_t *t, int size)
  HPX_NON_NULL(1);

#endif // LIBHPX_TRANSPORT_H
