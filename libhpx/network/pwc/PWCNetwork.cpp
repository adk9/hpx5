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
}

pwc_network_t* PWCNetwork::impl_ = NULL;

pwc_network_t&
PWCNetwork::Impl() {
  assert(impl_);
  return *impl_;
}

PWCNetwork::PWCNetwork(const config_t *cfg, boot_t *boot, gas_t *gas)
    : string_((gas->type == HPX_GAS_AGAS) ?
              static_cast<StringOps*>(new ParcelStringOps()) :
              static_cast<StringOps*>(new DMAStringOps(*this, boot_rank(boot))))
{
  assert(!impl_);
  impl_ = network_pwc_funneled_new(cfg, boot, gas);
}

PWCNetwork::~PWCNetwork()
{
  delete string_;
  pwc_deallocate(impl_);
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
  pwc_flush(impl_);
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
  return pwc_send(impl_, p, ssync);
}

void
PWCNetwork::pin(const void *base, size_t bytes, void *key)
{
  pwc_register_dma(impl_, base, bytes, key);
}

void
PWCNetwork::unpin(const void *base, size_t bytes)
{
  pwc_release_dma(impl_, base, bytes);
}

int
PWCNetwork::wait(hpx_addr_t lco, int reset)
{
  return pwc_lco_wait(impl_, lco, reset);
}

int
PWCNetwork::get(hpx_addr_t lco, size_t n, void *to, int reset)
{
  return pwc_lco_get(impl_, lco, n, to, reset);
}

int
PWCNetwork::init(void **collective)
{
  return pwc_coll_init(impl_, collective);
}

int
PWCNetwork::sync(void *in, size_t in_size, void* out, void *collective)
{
  return pwc_coll_sync(impl_, in, in_size, out, collective);
}

void
PWCNetwork::put(hpx_addr_t to, const void *lva, size_t n, const Command& lcmd,
                const Command& rcmd)
{
  if (pwc_put(impl_, to, lva, n, lcmd, rcmd)) {
    throw std::exception();
  }
}

void
PWCNetwork::get(void *lva, hpx_addr_t from, size_t n, const Command& lcmd,
                const Command& rcmd)
{
  if (pwc_get(impl_, lva, from, n, lcmd, rcmd)) {
    throw std::exception();
  }
}

void
PWCNetwork::cmd(int rank, const Command& lcmd, const Command& rcmd)
{
  if (pwc_cmd(impl_, rank, lcmd, rcmd)) {
    throw std::exception();
  }
}

