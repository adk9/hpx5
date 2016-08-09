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

ReloadParcelEmulator::P2P::P2P()
    : remote_(),
      send_(),
      recv_(nullptr)
{
}

ReloadParcelEmulator::P2P::~P2P()
{
  delete recv_;
}

int
ReloadParcelEmulator::P2P::send(unsigned rank, const hpx_parcel_t *p)
{
  size_t n = parcel_size(p);
  if (send_.put(rank, p, n)) {
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
ReloadParcelEmulator::P2P::reload(unsigned rank, size_t n, size_t eagerSize)
{
  if (recv_ && n) {
    recv_->deallocate(n);
  }

  recv_ = new(eagerSize) InplaceBlock(eagerSize);

  Op op;
  op.rank = rank;
  op.n = sizeof(EagerBlock);                    // only copy EeagerBlock data
  op.dest = remote_.addr;
  op.dest_key = &remote_.key;
  op.src = recv_;
  op.src_key = PhotonTransport::FindKeyRef(recv_, eagerSize);
  op.lop = Command::Nop();
  op.rop = Command::ReloadReply();

  dbg_check( op.put() );
}

void
ReloadParcelEmulator::P2P::init(unsigned rank, const Remote<P2P>& remote)
{
  remote_.addr = &remote.addr[rank].send_;
  remote_.key = remote.key;
}

int
ReloadParcelEmulator::send(unsigned rank, const hpx_parcel_t *p)
{
  return ends_[rank].send(rank, p);
}

void
ReloadParcelEmulator::deallocate(const hpx_parcel_t* p)
{
  InplaceBlock::DeleteParcel(p);
}

ReloadParcelEmulator::~ReloadParcelEmulator()
{
  PhotonTransport::Unpin(&ends_[0], ranks_ * sizeof(P2P));
}

ReloadParcelEmulator::ReloadParcelEmulator(const config_t *cfg, boot_t *boot)
    : rank_(boot_rank(boot)),
      ranks_(boot_n_ranks(boot)),
      eagerSize_(cfg->pwc_parcelbuffersize),
      ends_(new P2P[ranks_]())
{
  Remote<P2P> local;
  PhotonTransport::Pin(&ends_[0], ranks_ * sizeof(P2P), &local.key);
  local.addr = &ends_[0];

  std::unique_ptr<Remote<P2P>[]> remotes(new Remote<P2P>[ranks_]);
  boot_allgather(boot, &local, &remotes[0], sizeof(Remote<P2P>));
  for (int i = 0, e = ranks_; i < e; ++i) {
    ends_[i].init(rank_, remotes[i]);
  }
}

void
ReloadParcelEmulator::reload(unsigned src, size_t n)
{
  ends_[src].reload(src, n, eagerSize_);
}
