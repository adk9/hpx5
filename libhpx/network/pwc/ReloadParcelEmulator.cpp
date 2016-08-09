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
#include "libhpx/gpa.h"
#include "libhpx/libhpx.h"
#include "libhpx/padding.h"

namespace {
using libhpx::network::pwc::Command;
using libhpx::network::pwc::PhotonTransport;
using libhpx::network::pwc::P2P;
template <typename T>
using Remote = libhpx::network::pwc::Remote<T>;
using libhpx::network::pwc::ReloadParcelEmulator;
using Op = libhpx::network::pwc::PhotonTransport::Op;
}

P2P::P2P()
    : lock_(),
      rank_(),
      heapSegment_(),
      remoteSend_(),
      sendEager_(),
      sendBuffer_(sizeof(hpx_parcel_t*), 8),
      recv_(nullptr)
{
}

P2P::~P2P()
{
  sendBuffer_.fini();
  delete recv_;
}

void
P2P::init(unsigned rank, unsigned here, const Remote<P2P>& remote,
          const Remote<char>& heap)
{
  std::lock_guard<std::mutex> _(lock_);
  rank_ = rank;
  remoteSend_.addr = &remote.addr[here].sendEager_;
  remoteSend_.key = remote.key;
  heapSegment_.addr = heap.addr;
  heapSegment_.key = heap.key;
}

void
P2P::append(const hpx_parcel_t *p)
{
  auto **r = static_cast<const hpx_parcel_t**>(sendBuffer_.append());
  dbg_assert_str(r, "could not append a send operation to the buffer\n");
  *r = p;
}

int
P2P::start(const hpx_parcel_t* p)
{
  size_t n = parcel_size(p);
  if (sendEager_.put(rank_, p, n)) {
    return LIBHPX_OK;
  }

  Op op;
  op.rank = rank_;
  op.lop = Command::Nop();
  op.rop = Command::ReloadRequest(n);
  dbg_check( op.cmd() );
  return LIBHPX_RETRY;
}

int
P2P::Start(P2P* p2p, const hpx_parcel_t** p)
{
  return p2p->start(*p);
}

void
P2P::progress()
{
  std::lock_guard<std::mutex> _(lock_);
  typedef int (*ProgressCallback)(void*, void*);
  auto f = reinterpret_cast<ProgressCallback>(Start);
  int i = sendBuffer_.progress(f, this);
  if (i < 0) {
    dbg_error("failed to progress the send buffer\n");
  }
}

void
P2P::send(const hpx_parcel_t *p)
{
  std::lock_guard<std::mutex> _(lock_);
  if (sendBuffer_.size() || start(p) == LIBHPX_RETRY) {
    append(p);
  }
}

void
P2P::reload(size_t n, size_t eagerSize)
{
  if (recv_ && n) {
    recv_->deallocate(n);
  }

  recv_ = new(eagerSize) InplaceBlock(eagerSize);

  Op op;
  op.rank = rank_;
  op.n = sizeof(EagerBlock);                    // only copy EeagerBlock data
  op.dest = remoteSend_.addr;
  op.dest_key = &remoteSend_.key;
  op.src = recv_;
  op.src_key = PhotonTransport::FindKeyRef(recv_, eagerSize);
  op.lop = Command::Nop();
  op.rop = Command::ReloadReply();

  dbg_check( op.put() );
}

void
P2P::put(hpx_addr_t to, const void *lva, size_t n, const Command& lcmd,
         const Command& rcmd)
{
  Op op;
  op.rank = rank_;
  op.n = n;
  op.dest = heapSegment_.addr + gpa_to_offset(to);
  op.dest_key = &heapSegment_.key;
  op.src = lva;
  op.src_key = PhotonTransport::FindKeyRef(lva, n);
  op.lop = lcmd;
  op.rop = rcmd;
  dbg_check( op.put() );
}

void
P2P::get(void *lva, hpx_addr_t from, size_t n, const Command& lcmd,
         const Command& rcmd)
{
  Op op;
  op.rank = rank_;
  op.n = n;
  op.dest = lva;
  op.dest_key = PhotonTransport::FindKeyRef(lva, n);
  op.src = heapSegment_.addr + gpa_to_offset(from);
  op.src_key = &heapSegment_.key;
  op.lop = lcmd;
  op.rop = rcmd;
  dbg_check( op.get() );
}

int
ReloadParcelEmulator::send(unsigned rank, const hpx_parcel_t *p)
{
  ends_[rank].send(p);
  return LIBHPX_OK;
}

ReloadParcelEmulator::~ReloadParcelEmulator()
{
  PhotonTransport::Unpin(&ends_[0], 0);
  //PhotonTransport::Unpin(gas_local_base(gas_), 0);
}

ReloadParcelEmulator::ReloadParcelEmulator(const config_t *cfg, boot_t *boot,
                                           gas_t* gas)
    : ranks_(boot_n_ranks(boot)),
      eagerSize_(cfg->pwc_parcelbuffersize),
      ends_(new P2P[ranks_]())
{
  struct Exchange {
    Remote<P2P> p2p;
    Remote<char> heap;
  } local;

  local.p2p.addr = &ends_[0];
  PhotonTransport::Pin(local.p2p.addr, ranks_ * sizeof(P2P), &local.p2p.key);

  if (gas->type == HPX_GAS_PGAS) {
    size_t n = gas_local_size(gas);
    local.heap.addr = static_cast<char*>(gas_local_base(gas));
    PhotonTransport::Pin(local.heap.addr, n, &local.heap.key);
  }

  std::unique_ptr<Exchange[]> remotes(new Exchange[ranks_]);
  boot_allgather(boot, &local, &remotes[0], sizeof(Exchange));

  unsigned rank = boot_rank(boot);
  for (int i = 0, e = ranks_; i < e; ++i) {
    ends_[i].init(i, rank, remotes[i].p2p, remotes[i].heap);
  }
}

void
ReloadParcelEmulator::reload(unsigned src, size_t n)
{
  ends_[src].reload(n, eagerSize_);
}
