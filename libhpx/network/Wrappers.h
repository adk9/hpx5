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

#ifndef LIBHPX_NETWORK_WRAPPERS_H
#define LIBHPX_NETWORK_WRAPPERS_H

#include "libhpx/Network.h"
#include "libsync/queues.hpp"
#include <atomic>

namespace libhpx {
namespace network {
class NetworkWrapper : public Network
{
 public:
  virtual ~NetworkWrapper();

  int
  type() const
  {
    return impl_->type();
  }
  void
  progress(int n)
  {
    impl_->progress(n);
  }

  hpx_parcel_t*
  probe(int n)
  {
    return impl_->probe(n);
  }

  void
  flush()
  {
    impl_->flush();
  }

  CollectiveOps&
  collectiveOpsProvider()
  {
    return impl_->collectiveOpsProvider();
  }

  LCOOps&
  lcoOpsProvider()
  {
    return impl_->lcoOpsProvider();
  }

  MemoryOps&
  memoryOpsProvider()
  {
    return impl_->memoryOpsProvider();
  }

  ParcelOps&
  parcelOpsProvider()
  {
    return impl_->parcelOpsProvider();
  }

  StringOps&
  stringOpsProvider()
  {
    return impl_->stringOpsProvider();
  }

 protected:
  NetworkWrapper(Network* impl);

 private:
  Network* impl_;
};

class InstrumentationWrapper final : public NetworkWrapper, public ParcelOps {
 public:
  InstrumentationWrapper(Network* impl);

  void progress(int n);
  hpx_parcel_t* probe(int);
  int send(hpx_parcel_t* p, hpx_parcel_t* ssync);
  ParcelOps& parcelOpsProvider();

 private:
  ParcelOps& next_;
};

class CompressionWrapper final : public NetworkWrapper, public ParcelOps {
 public:
  CompressionWrapper(Network* impl);
  int send(hpx_parcel_t* p, hpx_parcel_t* ssync);
  ParcelOps& parcelOpsProvider();

 private:
  ParcelOps& next_;
};

class CoalescingWrapper final : public NetworkWrapper, public ParcelOps {
 public:
  CoalescingWrapper(Network* impl, const config_t *cfg, gas_t *gas);
  void progress(int n);
  void flush();
  int send(hpx_parcel_t* p, hpx_parcel_t* ssync);
  ParcelOps& parcelOpsProvider();

 private:
  void send(unsigned n);

  ParcelOps&                  next_;
  gas_t* const                 gas_;
  const unsigned              size_;
  std::atomic<unsigned>       prev_;
  std::atomic<unsigned>      count_;
  std::atomic<unsigned> coalescing_;
  libsync::TwoLockQueue<hpx_parcel_t*> sends_;
};

} // namespace network
} // namespace libhpx

#endif //  LIBHPX_NETWORK_WRAPPERS_H
