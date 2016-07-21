// ==================================================================-*- C++ -*-
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2016, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================

#ifndef LIBHPX_NETWORK_PWC_SEND_BUFFER_H
#define LIBHPX_NETWORK_PWC_SEND_BUFFER_H


#include "CircularBuffer.h"
#include "parcel_emulation.h"
#include "xport.h"
#include <libsync/locks.h>

extern "C" struct hpx_parcel;

namespace libhpx {
namespace network {
namespace pwc {

struct send_buffer_t {
  struct tatas_lock    lock;
  unsigned             rank;
  int        UNUSED_PADDING;
  parcel_emulator_t   *emul;
  pwc_xport_t        *xport;
  CircularBuffer    pending;
};

/// Initialize a send buffer.
int send_buffer_init(send_buffer_t *sends, unsigned rank,
                     parcel_emulator_t *emul, pwc_xport_t *xport,
                     uint32_t size);

/// Finalize a send buffer.
void send_buffer_fini(send_buffer_t *sends);

/// Perform a parcel send operation.
///
/// The parcel send is a fundamentally asynchronous operation. If @p lsync is
/// not HPX_NULL then it will be signaled when the local send operation
/// completes at the network level.
///
/// This send operation must be properly synchronized with other sends, and with
/// send_buffer_progress() since they may be called concurrently from more than
/// one thread.
///
/// NB: We don't currently support the @p lsync operation. When a send
///     completes, it generates a local completion event that is returned
///     through a local probe (local progress) operation. This completion event
///     will delete the parcel.
///
/// @param        sends The send buffer.
/// @param            p The parcel to send.
///
/// @returns  LIBHPX_OK The send operation was successful (i.e., it was passed
///                       to the network or it was buffered).
///       LIBHPX_ENOMEM The send operation could not complete because it needed
///                       to be buffered but the system was out of memory.
///        LIBHPX_ERROR An unexpected error occurred.
int send_buffer_send(send_buffer_t *sends, const struct hpx_parcel *p);

int send_buffer_progress(send_buffer_t *sends);

} // namespace pwc
} // namespace network
} // namespace libhpx

#endif // LIBHPX_NETWORK_PWC_EAGER_BUFFER_H
