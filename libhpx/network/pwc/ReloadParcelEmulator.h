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

#include "xport.h"
#include "libhpx/boot.h"
#include "libhpx/config.h"
#include "libhpx/parcel.h"
#include "libhpx/parcel_block.h"
#include <memory>

namespace libhpx {
namespace network {
namespace pwc {
class ReloadParcelEmulator {
 public:
  ReloadParcelEmulator(const config_t *cfg, boot_t *boot, pwc_xport_t& xport);
  ~ReloadParcelEmulator();

  int send(unsigned rank, const hpx_parcel_t *p);
  void reload(unsigned src, size_t n);

 private:
  /// An individual eager buffer representation.
  class EagerBuffer {
   public:
    void init(size_t n, pwc_xport_t& xport);
    void fini();
    void reload(size_t n, pwc_xport_t& xport);
    int send(pwc_xport_t& xport, xport_op_t& op);

   private:
    size_t capacity_;
    size_t next_;
    parcel_block_t* block_;
    xport_key_t key_;
  };

  /// An rdma-able remote address.
  struct Remote {
    void      *addr;
    xport_key_t key;
  };

  unsigned rank_;                              //<! our rank here
  unsigned ranks_;                             //<! number of ranks
  pwc_xport_t& xport_;                         //<! the transport
  std::unique_ptr<EagerBuffer[]> recvBuffers_; //<!
  xport_key_t recvBuffersKey_;                 //<!
  std::unique_ptr<EagerBuffer[]> sendBuffers_; //<!
  xport_key_t sendBuffersKey_;                 //<!
  std::unique_ptr<Remote[]> remotes_;          //<!
};

}
} // namespace network
} // namespace libhpx

#endif // LIBHPX_NETWORK_PWC_RELOAD_PARCEL_EMULATOR_H
