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
#include "libhpx/parcel.h"
#include <mutex>

namespace libhpx {
namespace network {
namespace pwc {

class SendBuffer {
 public:
  SendBuffer();


  /// Initialize a send buffer.
  void init(unsigned rank, parcel_emulator_t *emul, pwc_xport_t *xport);

  /// Finalize a send buffer.
  void fini();

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
  int send(const hpx_parcel_t* p);

  /// Progress a send buffer.
  ///
  /// Progressing a send buffer means transferring as many buffered sends to the
  /// network as is currently possible. This will return the number of remaining
  /// buffered sends.
  ///
  /// Progressing a send buffer must be properly synchronized with the send
  /// operation, as well as with concurrent attempts to progress the buffer, since
  /// they may be called concurrently from more than one thread.
  ///
  /// @param        sends The send buffer.
  ///
  /// @returns            HPX_SUCCESS or an error code.
  int progress();

 private:
  int start(const hpx_parcel_t *p);

  static int StartRecord(void *buffer, void *record);

  /// Append a record to the parcel's pending send buffer.
  ///
  /// @param        sends The send buffer.
  /// @param            p The parcel to buffer.
  ///
  /// @returns  LIBHXP_OK The parcel was buffered successfully.
  ///        LIBHPX_ERROR A pending record could not be allocated.
  int append(const hpx_parcel_t *p);

  std::mutex    lock_;
  unsigned             rank_;
  const int UNUSED_PADDING_;
  parcel_emulator_t   *emul_;
  pwc_xport_t        *xport_;
  CircularBuffer    pending_;
};

} // namespace pwc
} // namespace network
} // namespace libhpx

#endif // LIBHPX_NETWORK_PWC_EAGER_BUFFER_H
