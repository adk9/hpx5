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
      xport_(pwc_xport_new(cfg, boot, gas)),
      parcels_(cfg, boot, *xport_),
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
    xport_->pin(local.base, local.n, local.key);
    boot_allgather(boot, &local, segments_, sizeof(HeapSegment));
  }

  // Initialize the send buffers.
  for (int i = 0, e = ranks_; i < e; ++i) {
    sendBuffers_[i].init(i, parcels_, xport_);
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
      xport_->test(&command, &remaining, XPORT_ANY_SOURCE, &src);
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
    xport_->unpin(gas_local_base(gas_), gas_local_size(gas_));
    delete [] segments_;
  }

  delete [] sendBuffers_;
  delete string_;
  xport_->dealloc(xport_);
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
    while (xport_->test(&command, nullptr, XPORT_ANY_SOURCE, &src)) {
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
    while (xport_->probe(&command, nullptr, XPORT_ANY_SOURCE, &src)) {
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
  xport_->pin(base, bytes, key);
}

void
PWCNetwork::unpin(const void *base, size_t bytes)
{
  xport_->unpin(base, bytes);
}

int
PWCNetwork::init(void **collective)
{
  return LIBHPX_OK;
}

int
PWCNetwork::sync(void *in, size_t in_size, void* out, void *collective)
{
  coll_t *c = (coll_t*)collective;
  void *sendbuf = in;
  int count = in_size;
  char *comm = c->data + c->group_bytes;

  // flushing network is necessary (sufficient ?) to execute any packets
  // destined for collective operation
  flush();

  if (c->type == ALL_REDUCE) {
    xport_->allreduce(sendbuf, out, count, NULL, &c->op, comm);
  }
  return LIBHPX_OK;
}

void
PWCNetwork::put(hpx_addr_t to, const void *lva, size_t n, const Command& lcmd,
                const Command& rcmd)
{
  int rank = gpa_to_rank(to);

  xport_op_t op;
  op.rank = rank;
  op.n = n;
  op.dest = segments_[rank].base + gpa_to_offset(to);
  op.dest_key = &segments_[rank].key;
  op.src = lva;
  op.src_key = xport_->key_find_ref(xport_, lva, n);
  op.lop = lcmd;
  op.rop = rcmd;
  if (xport_->pwc(&op)) {
    throw std::exception();
  }
}

void
PWCNetwork::get(void *lva, hpx_addr_t from, size_t n, const Command& lcmd,
                const Command& rcmd)
{
  int rank = gpa_to_rank(from);

  xport_op_t op;
  op.rank = rank;
  op.n = n;
  op.dest = lva;
  op.dest_key = xport_->key_find_ref(xport_, lva, n);
  op.src = segments_[rank].base + gpa_to_offset(from);
  op.src_key = &segments_[rank].key;
  op.lop = lcmd;
  op.rop = rcmd;
  if (xport_->gwc(&op)) {
    throw std::exception();
  }
}

void
PWCNetwork::cmd(int rank, const Command& lcmd, const Command& rcmd)
{
  if (xport_->cmd(rank, lcmd, rcmd)) {
    throw std::exception();
  }
}

