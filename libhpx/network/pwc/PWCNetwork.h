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

#ifndef LIBHPX_NETWORK_PWC_PWC_NETWORK_H
#define LIBHPX_NETWORK_PWC_PWC_NETWORK_H

#include "libhpx/Network.h"
#include "Commands.h"
#include "Peer.h"
#include "PhotonTransport.h"
#include "libhpx/parcel.h"
#include <mutex>

namespace libhpx {
namespace network {
namespace pwc {
class PWCNetwork final : public Network, public CollectiveOps, public LCOOps,
                         public MemoryOps, public ParcelOps
{
 public:
  /// Allocate a PWCNetwork instance.
  PWCNetwork(const config_t *cfg, boot_t *boot, gas_t *gas);

  /// Delete a PWNetwork instace.
  ~PWCNetwork();

  /// A new operator that makes sure that the network instance is aligned to a
  /// cacheline boundary. A side effect of this operator is to initialize
  /// the Photon transport.
  static void* operator new (size_t size);

  /// The delete operator matches the new operator.
  static void operator delete (void *p);

  int type() const;
  void progress(int);
  hpx_parcel_t* probe(int);
  void flush();

  CollectiveOps& collectiveOpsProvider();
  LCOOps& lcoOpsProvider();
  MemoryOps& memoryOpsProvider();
  ParcelOps& parcelOpsProvider();
  StringOps& stringOpsProvider();

  void deallocate(const hpx_parcel_t* p);
  int send(hpx_parcel_t* p, hpx_parcel_t* ssync);

  void pin(const void *base, size_t bytes, void *key);
  void unpin(const void *base, size_t bytes);

  int wait(hpx_addr_t lco, int reset);
  int get(hpx_addr_t lco, size_t n, void *to, int reset);

  int init(void **collective);
  int sync(void *in, size_t in_size, void* out, void *collective);

  void put(hpx_addr_t to, const void *lva, size_t n, const Command& lcmd,
           const Command& rcmd);

  void get(void *lva, hpx_addr_t from, size_t n, const Command& lcmd,
           const Command& rcmd);

  /// Reload an eager buffer.
  void reload(unsigned src, size_t n);

  /// Progress the send buffer for a particular rank.
  void progressSends(unsigned rank);

  static PWCNetwork& Instance();

 private:
  /// Perform a rendezvous parcel send operation.
  ///
  /// For normal size parcels, we use the set of one-to-one pre-allocated eager
  /// parcel buffers to put the parcel data directly to the target rank. For
  /// larger parcels that will either always overflow the eager buffer, or that
  /// will use them up quickly and cause lots of re-allocation synchronization, we
  /// use this rendezvous protocol.
  ///
  /// @param            p The parcel to send.
  ///
  /// @returns            The status of the operation.
  int rendezvousSend(const hpx_parcel_t* p);

  static PWCNetwork* Instance_;

  const unsigned     rank_;
  const unsigned    ranks_;
  const size_t  eagerSize_;
  StringOps*       string_;
  gas_t* const        gas_;
  boot_t* const      boot_;
  std::mutex progressLock_;
  std::mutex    probeLock_;
  std::unique_ptr<Peer[]> peers_;
};

} // namespace pwc
} // namespace network
} // namespace libhpx

#endif // LIBHPX_NETWORK_PWC_PWC_NETWORK_H
