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
#ifndef LIBHPX_NETWORK_PWC_EAGER_BUFFER_H
#define LIBHPX_NETWORK_PWC_EAGER_BUFFER_H

#include <stdint.h>
#include <hpx/hpx.h>

struct peer;

/// Represents an eager buffer that supports peer-to-peer active message sends.
///
/// This buffer supports both a tx and rx view. Each point-to-point buffer
/// represents a block of bytes residing on the target process. From the
/// sender's perspective, the base pointer is the remote virtual address of this
/// buffer, and from the receiver's perspective this is the local address.
///
/// Byte transfer from the sender to the receiver uses the peer's pwc() and
/// get() channel, driven by the sender. When it receives a new tx() request,
/// the sender checks to see if the send will overflow the size of the buffer,
/// based on its most recent understanding of the buffer's "last" value on the
/// target side. If it looks like an overflow, the sender will initiate a get()
/// operation to get a new value for last, and fail the send.
///
/// If the send looks like it can succeed, the sender will issue a pwc() to the
/// remote buffer location that contains an encoding to receive a parcel, and
/// the new "next" value.
///
/// If there is not enough space in the buffer, the sender will issue a pwc() to
/// the remote buffer that encodes the number of bytes to skip.
typedef struct eager_buffer {
  struct peer     *peer;
  // tatas_lock_t     lock; locking happens in the send buffer
  uint32_t         size;
  const uint32_t UNUSED;
  uint64_t     sequence;
  uint64_t          max;
  uint64_t          min;
  uint64_t      tx_base;
  char         *rx_base;
} eager_buffer_t;

/// Initialize an eager buffer.
///
/// @param       buffer The buffer to initialize.
/// @param      tx_base The base tx offset for the buffer.
/// @param      rx_base The base of the rx buffer.
/// @param         size The size of the buffer.
///
/// @returns  LIBHPX_OK The buffer was initialized correctly.
int eager_buffer_init(eager_buffer_t *buffer, struct peer *peer,
                      uint64_t tx_base, char *rx_base, uint32_t size)
  HPX_INTERNAL HPX_NON_NULL(1, 2);

/// Finalize a buffer.
void eager_buffer_fini(eager_buffer_t *buffer)
  HPX_INTERNAL HPX_NON_NULL(1);

/// Perform an eager parcel buffer send operation.
///
/// This only works on the remote endpoint for an eager buffer.
///
/// @param       buffer The eager buffer to send through.
/// @param            p The parcel to send.
/// @param        lsync An event identifier for local completion.
///
/// @returns  LIBHPX_OK The send completed successfully.
///        LIBHPX_RETRY There was not enough space in the buffer.
///        LIBHPX_ERROR There was another error during the send operation.
int eager_buffer_tx(eager_buffer_t *buffer, hpx_parcel_t *p)
  HPX_INTERNAL HPX_NON_NULL(1, 2);

/// Perform an eager parcel recv.
///
/// This only works on the local endpoint for an eager buffer.
hpx_parcel_t *eager_buffer_rx(eager_buffer_t *buffer)
  HPX_INTERNAL HPX_NON_NULL(1);

#endif // LIBHPX_NETWORK_PWC_EAGER_BUFFER_H
