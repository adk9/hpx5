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
#include "pwc.h"
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

using libhpx::network::pwc::pwc_network_t;
using libhpx::network::pwc::pwc_xport_t;
}

PWCNetwork* PWCNetwork::Instance_ = nullptr;

PWCNetwork& PWCNetwork::Instance() {
  assert(Instance_);
  return *Instance_;
}

pwc_network_t&
PWCNetwork::Impl() {
  return *Instance().impl_;
}

PWCNetwork::PWCNetwork(const config_t *cfg, boot_t *boot, gas_t *gas)
    : xport_(pwc_xport_new(cfg, boot, gas)),
      string_((gas->type == HPX_GAS_AGAS) ?
              static_cast<StringOps*>(new ParcelStringOps()) :
              static_cast<StringOps*>(new DMAStringOps(*this,
                                                       boot_rank(boot)))),
      impl_(network_pwc_funneled_new(cfg, boot, gas, xport_))
{
  assert(!Instance_);
  Instance_ = this;
}

PWCNetwork::~PWCNetwork()
{
  assert(Instance_);
  pwc_deallocate(impl_);
  delete string_;
  xport_->dealloc(xport_);
}

int
PWCNetwork::type() const
{
  return HPX_NETWORK_PWC;
}

void
PWCNetwork::progress(int n)
{
  pwc_progress(impl_, n);
}

void
PWCNetwork::flush()
{
}

hpx_parcel_t*
PWCNetwork::probe(int n)
{

  return pwc_probe(impl_, n);
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
    return pwc_send(impl_, p, ssync);
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
  op.dest = impl_->heap_segments[rank].base + gpa_to_offset(to);
  op.dest_key = &impl_->heap_segments[rank].key;
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
  op.src = impl_->heap_segments[rank].base + gpa_to_offset(from);
  op.src_key = &impl_->heap_segments[rank].key;
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

