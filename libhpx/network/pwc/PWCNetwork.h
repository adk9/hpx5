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
#include "commands.h"

namespace libhpx {
namespace network {
namespace pwc {
class pwc_network_t;

class PWCNetwork final : public Network, public CollectiveOps, public LCOOps,
                         public MemoryOps, public ParcelOps {
 public:
  PWCNetwork(const config_t *cfg, boot_t *boot, gas_t *gas);
  ~PWCNetwork();

  int type() const;
  void progress(int);
  hpx_parcel_t* probe(int);
  void flush();

  CollectiveOps& collectiveOpsProvider();
  LCOOps& lcoOpsProvider();
  MemoryOps& memoryOpsProvider();
  ParcelOps& parcelOpsProvider();
  StringOps& stringOpsProvider();

  int send(hpx_parcel_t* p, hpx_parcel_t* ssync);

  void pin(const void *base, size_t bytes, void *key);
  void unpin(const void *base, size_t bytes);

  int wait(hpx_addr_t lco, int reset);
  int get(hpx_addr_t lco, size_t n, void *to, int reset);

  int init(void **collective);
  int sync(void *in, size_t in_size, void* out, void *collective);

  /// Initiate an rDMA put operation with a remote continuation.
  ///
  /// This will copy @p n bytes between the @p lca and the @p to buffer, running
  /// the @p lcmd when the local buffer can be modified or deleted and the @p rcmd
  /// when the remote write has completed.
  ///
  /// @param           to The global target for the put.
  /// @param          lva The local source for the put.
  /// @param            n The number of bytes to put.
  /// @param         lcmd The local command, run when @p lva can be reused.
  /// @param         rcmd The remote command, run when @p to has be written.
  void put(hpx_addr_t to, const void *lva, size_t n, const Command& lcmd,
           const Command& rcmd);

  /// Initiate an rDMA get operation.
  ///
  /// This will copy @p n bytes between the @p from buffer and the @p lva, running
  /// the @p rcmd when the read completes remotely and running the @p lcmd when
  /// the local write is complete.
  ///
  /// @param          lva The local target for the get.
  /// @param         from The global source for the get.
  /// @param            n The number of bytes to get.
  /// @param         lcmd A local command, run when @p lva is written.
  /// @param         rcmd A remote command, run when @p from has been read.
  void get(void *lva, hpx_addr_t from, size_t n, const Command& lcmd,
           const Command& rcmd);

  /// Perform a PWC network command.
  ///
  /// This sends a "pure" command to the scheduler at a different rank, without
  /// any additional argument data. This can avoid using any additional eager
  /// parcel buffer space, and can always be satisfied with one low-level "put"
  /// operation.
  ///
  /// @param         rank The rank id to send the remote command to.
  /// @param         lcmd A command to be run for local completion.
  /// @param         rcmd A remote command to be run at @p rank.
  void cmd(int rank, const Command& lcmd, const Command& rcmd);

  static pwc_network_t& Impl();

 private:
  static pwc_network_t *impl_;
  StringOps* string_;
};


} // namespace pwc
} // namespace network
} // namespace libhpx

#endif // LIBHPX_NETWORK_PWC_PWC_NETWORK_H
