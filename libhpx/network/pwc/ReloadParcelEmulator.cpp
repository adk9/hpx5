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

void
ReloadParcelEmulator::EagerBuffer::fini()
{
  if (block_) {
    parcel_block_delete(block_);
  }
}

void
ReloadParcelEmulator::EagerBuffer::reload(size_t n)
{
  if (n) {
    dbg_assert(block_);
    parcel_block_deduct(block_, n);
  }
  block_ = parcel_block_new(capacity_, capacity_, &next_);
  PhotonTransport::FindKey(block_, capacity_, &key_);
}

void
ReloadParcelEmulator::EagerBuffer::init(size_t n)
{
  capacity_ = n;
  next_ = n;
}

int
ReloadParcelEmulator::EagerBuffer::send(Op& op)
{
  int i = next_;
  dbg_assert(!(i & 7));
  size_t r = capacity_ - i;
  if (op.n < r) {
    // make sure i stays 8-byte aligned
    size_t align = ALIGN(op.n, 8);
    next_ += op.n + align;
    log_parcel("allocating %zu bytes in buffer %p (%zu remain)\n",
               op.n + align, (void*)block_, capacity_ - next_);
    op.dest_key = &key_;
    op.dest = parcel_block_at(block_, i);
    op.rop = Command::RecvParcel(static_cast<hpx_parcel_t*>(op.dest));
    return op.put();
  }

  op.n = 0;
  op.src = nullptr;
  op.src_key = nullptr;
  op.lop = Command::Nop();
  op.rop = Command::ReloadRequest(r);
  int e = op.cmd();
  if (LIBHPX_OK == e) {
    return LIBHPX_RETRY;
  }

  dbg_error("could not complete send operation\n");
}

int
ReloadParcelEmulator::send(unsigned rank, const hpx_parcel_t *p)
{
  size_t n = parcel_size(p);
  Op op;
  op.rank = rank;
  op.n = n;
  op.dest = nullptr;
  op.dest_key = nullptr;
  op.src = p;
  op.src_key = PhotonTransport::FindKeyRef(p, n);
  op.lop = Command::DeleteParcel(p);
  op.rop = Command::Nop();

  if (!op.src_key) {
    dbg_error("no rdma key for local parcel (%p, %zu)\n", (void*)p, n);
  }

  return sendBuffers_[rank].send(op);
}

ReloadParcelEmulator::~ReloadParcelEmulator()
{
  for (int i = 0, e = ranks_; i < e; ++i) {
    recvBuffers_[i].fini();
  }
  PhotonTransport::Unpin(recvBuffers_.get(), ranks_ * sizeof(EagerBuffer));
  PhotonTransport::Unpin(sendBuffers_.get(), ranks_ * sizeof(EagerBuffer));
}

ReloadParcelEmulator::ReloadParcelEmulator(const config_t *cfg, boot_t *boot)
    : rank_(boot_rank(boot)),
      ranks_(boot_n_ranks(boot)),
      recvBuffers_(new EagerBuffer[ranks_]),
      recvBuffersKey_(),
      sendBuffers_(new EagerBuffer[ranks_]),
      sendBuffersKey_(),
      remotes_(new Remote[ranks_])
{
  size_t eagerBytes = ranks_ * sizeof(EagerBuffer);
  PhotonTransport::Pin(recvBuffers_.get(), eagerBytes, &recvBuffersKey_);
  PhotonTransport::Pin(sendBuffers_.get(), eagerBytes, &sendBuffersKey_);

  // Initialize the recv buffers for this rank.
  for (int i = 0, e = ranks_; i < e; ++i) {
    recvBuffers_[i].init(cfg->pwc_parcelbuffersize);
  }

  // Initialize a temporary array of remote pointers for this rank's sends.
  std::unique_ptr<Remote[]> temp(new Remote[ranks_]);
  for (int i = 0, e = ranks_; i < e; ++i) {
    temp[i].addr = &sendBuffers_[i];
    temp[i].key = sendBuffersKey_;
  }

  // exchange all of the recv buffers, and all of the remote send pointers
  boot_alltoall(boot, sendBuffers_.get(), recvBuffers_.get(),
                sizeof(EagerBuffer), sizeof(EagerBuffer));
  boot_alltoall(boot, remotes_.get(), temp.get(), sizeof(Remote),
                sizeof(Remote));
}

void
ReloadParcelEmulator::reload(unsigned src, size_t n)
{
  recvBuffers_[src].reload(n);

  Op op;
  op.rank = src;
  op.n = sizeof(EagerBuffer);
  op.dest = remotes_[src].addr;
  op.dest_key = &remotes_[src].key;
  op.src = &recvBuffers_[src];
  op.src_key = &recvBuffersKey_;
  op.lop = Command::Nop();
  op.rop = Command::ReloadReply();

  dbg_check( op.put() );
}
