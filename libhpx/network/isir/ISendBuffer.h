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

#ifndef LIBHPX_NETWORK_ISIR_ISEND_BUFFER_H
#define LIBHPX_NETWORK_ISIR_ISEND_BUFFER_H

#include "MPITransport.h"

extern "C" struct gas;
extern "C" struct hpx_parcel;

namespace libhpx {
namespace network {
namespace isir {
class ISendBuffer {
  using Transport = libhpx::network::isir::MPITransport;
  using Request = Transport::Request;

 public:
  /// Allocate a send buffer.
  ///
  /// @param          gas The global address space.
  /// @param        xport The isir xport to use.
  /// @param        limit The limit of the number of active requests.
  /// @param         twin The initial number of requests tested.
  ISendBuffer(struct gas* gas, Transport &xport, unsigned limit, unsigned twin);

  /// Finalize a send buffer.
  ~ISendBuffer();

  /// Append a send to the buffer.
  ///
  /// This may or may not start the send immediately.
  ///
  /// @param            p The stack of parcels to send.
  /// @param        ssync The stack of parcel continuations.
  void append(struct hpx_parcel *p, struct hpx_parcel *ssync);

  /// Progress the sends in the buffer.
  ///
  /// @param[out]   ssync A stack of synchronization parcels.
  ///
  /// @returns            The number of completed requests.
  int progress(struct hpx_parcel **ssync);

  /// Flush all outstanding sends.
  ///
  /// This is synchronous and will not return until all of the buffered sends and
  /// sends in the parcel queue have completed. It is not thread safe.
  ///
  /// @param[out]   ssync A stack of synchronization parcels.
  ///
  /// @returns            The number of completed requests during the flush.
  int flush(struct hpx_parcel **ssync);

 private:
  struct Record {
    struct hpx_parcel *parcel;
    struct hpx_parcel  *ssync;
  };

  static int PayloadSizeToTag(unsigned payload);

  void reserve(unsigned size);
  void compact(unsigned long n, const int out[]);

  void start(unsigned long i);
  unsigned long startAll();

  /// Cancel an active request.
  ///
  /// This is synchronous, and will wait until the request has been canceled.
  ///
  /// @param           id The isend to cancel.
  /// @param[out] parcels Any canceled parcels.
  void cancel(unsigned long id, struct hpx_parcel **parcels);

  /// Cancel and cleanup all outstanding requests in the buffer.
  ///
  /// @returns Any canceled parcels.
  struct hpx_parcel* cancelAll();

  unsigned testRange(unsigned i, unsigned n, int* out,
                     struct hpx_parcel** ssync);
  unsigned long testAll(struct hpx_parcel** ssync);

  struct gas*      gas_;
  Transport&     xport_;
  unsigned       limit_;
  unsigned        twin_;
  unsigned        size_;
  unsigned long    min_;
  unsigned long active_;
  unsigned long    max_;
  Request*    requests_;
  Record*      records_;
};
} // namespace isir
} // namespace network
} // namespace libhpx

#endif // LIBHPX_NETWORK_ISIR_ISEND_BUFFER_H
