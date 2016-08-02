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

#include "Wrappers.h"
#include "libhpx/events.h"

namespace {
using libhpx::ParcelOps;
using libhpx::network::NetworkWrapper;
using libhpx::network::InstrumentationWrapper;
}

InstrumentationWrapper::InstrumentationWrapper(Network* impl)
    : NetworkWrapper(impl),
      next_(impl->parcelOpsProvider())
{
}

void
InstrumentationWrapper::progress(int n)
{
  EVENT_NETWORK_PROGRESS_BEGIN();
  NetworkWrapper::progress(n);
  EVENT_NETWORK_PROGRESS_END();
}

int
InstrumentationWrapper::send(hpx_parcel_t *p, hpx_parcel_t *ssync)
{
  EVENT_NETWORK_SEND();
  return next_.send(p, ssync);
}

hpx_parcel_t*
InstrumentationWrapper::probe(int nrx)
{
  EVENT_NETWORK_PROBE_BEGIN();
  hpx_parcel_t *p = NetworkWrapper::probe(nrx);
  EVENT_NETWORK_PROBE_END();
  return p;
}

ParcelOps&
InstrumentationWrapper::parcelOpsProvider()
{
  return *this;
}
