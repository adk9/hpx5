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

#include <cinttypes>
#include <cuckoohash_map.hh>
#include <city_hasher.hh>
#include <hpx/hpx.h>
#include <libhpx/debug.h>
#include <libhpx/gas.h>
#include <libhpx/locality.h>
#include <libhpx/scheduler.h>
#include "cuckoo_hash.h"

using libhpx::gas::Affinity;
using libhpx::gas::CuckooHash;

CuckooHash::CuckooHash() : map_()
{
}

CuckooHash::~CuckooHash()
{
}

void
CuckooHash::set(hpx_addr_t gva, int worker)
{
  DEBUG_IF(gas_owner_of(here->gas, gva) != here->rank) {
    dbg_error("Attempt to set affinity of %" PRIu64 " at %d (owned by %d)\n",
              gva, here->rank, gas_owner_of(here->gas, gva));
  }
  DEBUG_IF(worker < 0 || here->sched->n_workers <= worker) {
    dbg_error("Attempt to set affinity of %" PRIu64
              " to %d is outside range [0, %d)\n",
              gva, worker, here->sched->n_workers);
  }
  // @todo: Should we be pinning gva? The interface doesn't require it, but it
  //        could prevent usage errors in AGAS? On the other hand, it could
  //        result in debugging issues with pin reference counting.
  map_.insert(gva, worker);
}

void
CuckooHash::clear(hpx_addr_t gva)
{
  DEBUG_IF(gas_owner_of(here->gas, gva) != here->rank) {
    dbg_error("Attempt to clear affinity of %" PRIu64 " at %d (owned by %d)\n",
              gva, here->rank, gas_owner_of(here->gas, gva));
  }
  map_.erase(gva);
}

int
CuckooHash::get(hpx_addr_t gva) const
{
  int worker = -1;
  map_.find(gva, worker);
  return worker;
}
