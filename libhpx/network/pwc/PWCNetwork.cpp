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

#include "PWCNetwork.h"
#include "DMAStringOps.h"
#include "libhpx/collective.h"
#include "libhpx/gpa.h"
#include "libhpx/libhpx.h"
#include "libhpx/ParcelStringOps.h"
#include <exception>

namespace {
using libhpx::CollectiveOps;
using libhpx::LCOOps;
using libhpx::MemoryOps;
using libhpx::ParcelOps;
using libhpx::StringOps;
using libhpx::network::ParcelStringOps;
using libhpx::network::pwc::PWCNetwork;
using libhpx::network::pwc::DMAStringOps;
using Op = libhpx::network::pwc::PhotonTransport::Op;
using Key = libhpx::network::pwc::PhotonTransport::Key;
constexpr int ANY_SOURCE = libhpx::network::pwc::PhotonTransport::ANY_SOURCE;
}

PWCNetwork* PWCNetwork::Instance_ = nullptr;

PWCNetwork& PWCNetwork::Instance()
{
  assert(Instance_);
  return *Instance_;
}

PWCNetwork::PWCNetwork(const config_t *cfg, boot_t *boot, gas_t *gas)
    : rank_(boot_rank(boot)),
      ranks_(boot_n_ranks(boot)),
      parcels_(cfg, boot),
      string_((gas->type == HPX_GAS_AGAS) ?
              static_cast<StringOps*>(new ParcelStringOps()) :
              static_cast<StringOps*>(new DMAStringOps(*this,
                                                       boot_rank(boot)))),
      gas_(gas),
      boot_(boot),
      segments_(new HeapSegment[ranks_]),
  sendBuffers_(new SendBuffer[ranks_]),
  progressLock_(),
  probeLock_()
{
  assert(!Instance_);
  Instance_ = this;

  // Validate parameters.
  if (boot->type == HPX_BOOT_SMP) {
    log_net("will not instantiate PWC for the SMP boot network\n");
    throw std::exception();
  }

  // Validate configuration.
  if (popcountl(cfg->pwc_parcelbuffersize) != 1) {
    dbg_error("--hpx-pwc-parcelbuffersize must 2^k (given %zu)\n",
              cfg->pwc_parcelbuffersize);
  }

  if (cfg->pwc_parceleagerlimit > cfg->pwc_parcelbuffersize) {
    dbg_error(" --hpx-pwc-parceleagerlimit (%zu) must be less than "
              "--hpx-pwc-parcelbuffersize (%zu)\n",
              cfg->pwc_parceleagerlimit, cfg->pwc_parcelbuffersize);
  }

  if (gas->type == HPX_GAS_PGAS) {
    // All-to-all the heap segments
    HeapSegment local;
    local.n = gas_local_size(gas);
    local.base = (char*)gas_local_base(gas);
    PhotonTransport::Pin(local.base, local.n, &local.key);
    boot_allgather(boot, &local, segments_, sizeof(HeapSegment));
  }

  // Initialize the send buffers.
  for (int i = 0, e = ranks_; i < e; ++i) {
    sendBuffers_[i].init(i, parcels_);
  }
}

PWCNetwork::~PWCNetwork()
{
  // Cleanup any remaining local work---this can leak memory and stuff, because
  // we aren't actually running the commands that we cleanup.
  {
    std::lock_guard<std::mutex> _(progressLock_);
    int remaining, src;
    Command command;
    do {
      PhotonTransport::Test(&command, &remaining, ANY_SOURCE, &src);
    } while (remaining > 0);
  }

  // Network deletion is effectively a collective, so this enforces that
  // everyone is done with rdma before we go and deregister anything.
  boot_barrier(boot_);

  // Finalize the send buffers.
  for (int i = 0, e = ranks_; i < e; ++i) {
    sendBuffers_[i].fini();
  }

  // If we registered the heap segments then remove them.
  if (segments_) {
    PhotonTransport::Unpin(gas_local_base(gas_), gas_local_size(gas_));
    delete [] segments_;
  }

  delete [] sendBuffers_;
  delete string_;
  Instance_ = nullptr;
}

int
PWCNetwork::type() const
{
  return HPX_NETWORK_PWC;
}

void
PWCNetwork::progress(int n)
{
  if (auto _ = std::unique_lock<std::mutex>(progressLock_, std::try_to_lock)) {
    Command command;
    int src;
    while (PhotonTransport::Test(&command, nullptr, ANY_SOURCE, &src)) {
      command(rank_);
    }
  }
}

void
PWCNetwork::flush()
{
}

hpx_parcel_t*
PWCNetwork::probe(int n)
{
  if (auto _ = std::unique_lock<std::mutex>(probeLock_, std::try_to_lock)) {
    Command command;
    int src;
    while (PhotonTransport::Probe(&command, nullptr, ANY_SOURCE, &src)) {
      command(src);
    }
  }
  return nullptr;
}

CollectiveOps&
PWCNetwork::collectiveOpsProvider()
{
  return *this;
}

LCOOps&
PWCNetwork::lcoOpsProvider()
{
  return *this;
}

MemoryOps&
PWCNetwork::memoryOpsProvider()
{
  return *this;
}

ParcelOps&
PWCNetwork::parcelOpsProvider()
{
  return *this;
}

StringOps&
PWCNetwork::stringOpsProvider()
{
  return *string_;
}

int
PWCNetwork::send(hpx_parcel_t *p, hpx_parcel_t *ssync)
{
  // This is a blatant hack to keep track of the ssync parcel using p's next
  // pointer. It will allow us to both delete p and run ssync once the
  // underlying network operation is serviced. It works in conjunction with the
  // handle_delete_parcel command.
  dbg_assert(p->next == NULL);
  p->next = ssync;

  if (parcel_size(p) >= here->config->pwc_parceleagerlimit) {
    return rendezvousSend(p);
  }
  else {
    int rank = gas_owner_of(gas_, p->target);
    return sendBuffers_[rank].send(p);
  }
}

void
PWCNetwork::progressSends(unsigned rank)
{
  if (sendBuffers_[rank].progress()) {
    throw std::exception();
  }
}

void
PWCNetwork::pin(const void *base, size_t bytes, void *key)
{
  PhotonTransport::Pin(base, bytes, static_cast<Key*>(key));
}

void
PWCNetwork::unpin(const void *base, size_t bytes)
{
  PhotonTransport::Unpin(base, bytes);
}

int
PWCNetwork::init(void **collective)
{
  return LIBHPX_OK;
}

int
PWCNetwork::sync(void *in, size_t in_size, void* out, void *collective)
{
  return LIBHPX_OK;
}

void
PWCNetwork::put(hpx_addr_t to, const void *lva, size_t n, const Command& lcmd,
                const Command& rcmd)
{
  int rank = gpa_to_rank(to);

  Op op;
  op.rank = rank;
  op.n = n;
  op.dest = segments_[rank].base + gpa_to_offset(to);
  op.dest_key = &segments_[rank].key;
  op.src = lva;
  op.src_key = PhotonTransport::FindKeyRef(lva, n);
  op.lop = lcmd;
  op.rop = rcmd;
  if (op.put()) {
    throw std::exception();
  }
}

void
PWCNetwork::get(void *lva, hpx_addr_t from, size_t n, const Command& lcmd,
                const Command& rcmd)
{
  int rank = gpa_to_rank(from);

  Op op;
  op.rank = rank;
  op.n = n;
  op.dest = lva;
  op.dest_key = PhotonTransport::FindKeyRef(lva, n);
  op.src = segments_[rank].base + gpa_to_offset(from);
  op.src_key = &segments_[rank].key;
  op.lop = lcmd;
  op.rop = rcmd;
  if (op.get()) {
    throw std::exception();
  }
}

void
PWCNetwork::cmd(int rank, const Command& lcmd, const Command& rcmd)
{
  Op op;
  op.rank = rank;
  op.lop = lcmd;
  op.rop = rcmd;
  if (op.cmd()) {
    throw std::exception();
  }
}

void*
PWCNetwork::operator new (size_t size)
{
  PhotonTransport::Initialize(here->config, here->boot);
  void *memory;
  if (posix_memalign(&memory, HPX_CACHELINE_SIZE, size)) {
    dbg_error("Could not allocate aligned memory for the PWCNetwork\n");
    throw std::bad_alloc();
  }
  return memory;
}

void
PWCNetwork::operator delete (void *p)
{
  free(p);
}
