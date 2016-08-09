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

#ifndef LIBHPX_NETWORK_PWC_RELOAD_PARCEL_EMULATOR_H
#define LIBHPX_NETWORK_PWC_RELOAD_PARCEL_EMULATOR_H

#include "ParcelBlock.h"
#include "PhotonTransport.h"
#include "libhpx/boot.h"
#include "libhpx/config.h"
#include "libhpx/parcel.h"
#include <memory>

namespace libhpx {
namespace network {
namespace pwc {
class ReloadParcelEmulator {
 public:
  ReloadParcelEmulator(const config_t *cfg, boot_t *boot);
  ~ReloadParcelEmulator();

  /// Send a parcel to the associated rank.
  ///
  /// This operation will try and initiate an eager put of the parcel data into
  /// the receive buffer for the given rank. It will return LIBHPX_OK if it
  /// succeeds, or LIBHPX_RETRY if there is not enough space in the buffer. If
  /// there is not enough space it will initiate an asynchronous reload
  /// operation to the target rank.
  ///
  /// @param       rank The target rank.
  /// @param          p The serialized parcel data to send.
  ///
  /// @returns          LIBHPX_OK if it the send succeeds, or LIBHPX_RETRY if
  ///                   there was not enough space and a reload operation has
  ///                   been started.
  int send(unsigned rank, const hpx_parcel_t *p);

  /// Reload the designated rank.
  ///
  /// This handles a reload request. It will deduct @n bytes from the current
  /// InplaceBlock and allocate a new one, sending the new buffer information
  /// back to the source using the bakcpointers stored in the remotes_ array.
  ///
  /// @param        src The source of the reload request.
  /// @param          n A number of bytes to consume from the current block
  ///                   before replacing it.
  void reload(unsigned src, size_t n);

  /// Deallocate a block-allocated parcel.
  void deallocate(const hpx_parcel_t* p);

 private:
  /// An rdma-able remote address.
  ///
  /// We use an array of these remote addresses to keep track of the
  /// backpointers that we need during the reload operation. During reload we
  /// allocate a new InplaceBuffer and then rdma its data back to the remote
  /// address that we have stored for the requesting rank.
  template <typename T>
  struct Remote {
    T                  *addr;
    PhotonTransport::Key key;
  };

  class P2P {
   public:
    P2P();
    ~P2P();

    void init(unsigned rank, const Remote<P2P>& remote);

    int send(unsigned rank, const hpx_parcel_t* p);
    void reload(unsigned rank, size_t n, size_t eagerSize);

   private:
    Remote<EagerBlock> remote_;
    EagerBlock send_;
    InplaceBlock* recv_;
  };

  unsigned rank_;                                //<! our rank
  unsigned ranks_;                               //<! number of ranks
  size_t eagerSize_;                             //<! size of the buffers
  std::unique_ptr<P2P[]> ends_;
};
} // namespace pwc
} // namespace network
} // namespace libhpx

#endif // LIBHPX_NETWORK_PWC_RELOAD_PARCEL_EMULATOR_H
