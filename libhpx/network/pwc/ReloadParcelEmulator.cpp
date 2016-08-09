// =============================================================================
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "ReloadParcelEmulator.h"
#include "libhpx/debug.h"
#include "libhpx/libhpx.h"
#include "libhpx/padding.h"

namespace {
using libhpx::network::pwc::Command;
using libhpx::network::pwc::PhotonTransport;
using libhpx::network::pwc::ReloadParcelEmulator;
using Op = libhpx::network::pwc::PhotonTransport::Op;
}

int
ReloadParcelEmulator::send(unsigned rank, const hpx_parcel_t *p)
{
  size_t n = parcel_size(p);
  if (sendBuffers_[rank].put(rank, p, n)) {
    return LIBHPX_OK;                           // all bytes fit
  }

  Op op;
  op.rank = rank;
  op.n = 0;
  op.dest = nullptr;
  op.dest_key = nullptr;
  op.src = nullptr;
  op.src_key = nullptr;
  op.lop = Command::Nop();
  op.rop = Command::ReloadRequest(n);
  dbg_check( op.cmd() );
  return LIBHPX_RETRY;
}

void
ReloadParcelEmulator::deallocate(const hpx_parcel_t* p)
{
  InplaceBlock::DeleteParcel(p);
}

ReloadParcelEmulator::~ReloadParcelEmulator()
{
  for (int i = 0, e = ranks_; i < e; ++i) {
    delete recvBuffers_[i];
  }
}

ReloadParcelEmulator::ReloadParcelEmulator(const config_t *cfg, boot_t *boot)
    : rank_(boot_rank(boot)),
      ranks_(boot_n_ranks(boot)),
      eagerSize_(cfg->pwc_parcelbuffersize),
      recvBuffers_(new InplaceBlock*[ranks_]()),
      sendBuffers_(new EagerBlock[ranks_]),
      remotes_(new Remote[ranks_])
{
  // Initialize a temporary array of remote pointers for this rank's sends.
  auto extent = ranks_ * sizeof(Remote);
  auto key = PhotonTransport::FindKey(&sendBuffers_[0], extent);
  std::unique_ptr<Remote[]> temp(new Remote[ranks_]);
  for (int i = 0, e = ranks_; i < e; ++i) {
    temp[i].addr = &sendBuffers_[i];
    temp[i].key = key;
  }
  boot_alltoall(boot, &remotes_[0], &temp[0], sizeof(Remote), sizeof(Remote));
}

void
ReloadParcelEmulator::reload(unsigned src, size_t n)
{
  if (n && recvBuffers_[src]) {
    recvBuffers_[src]->deallocate(n);
  }

  recvBuffers_[src] = new(eagerSize_) InplaceBlock(eagerSize_);

  Op op;
  op.rank = src;
  op.n = sizeof(EagerBlock);                    // only copy EeagerBlock data
  op.dest = remotes_[src].addr;
  op.dest_key = &remotes_[src].key;
  op.src = recvBuffers_[src];
  op.src_key = PhotonTransport::FindKeyRef(recvBuffers_[src], eagerSize_);
  op.lop = Command::Nop();
  op.rop = Command::ReloadReply();

  dbg_check( op.put() );
}
