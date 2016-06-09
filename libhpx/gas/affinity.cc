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

#include <cuckoohash_map.hh>
#include <city_hasher.hh>
#include <hpx/hpx.h>
extern "C" {
#include <libhpx/debug.h>
#include <libhpx/gas.h>
#include <libhpx/locality.h>
#include <libhpx/scheduler.h>
}

namespace {
typedef cuckoohash_map<hpx_addr_t, int, CityHasher<hpx_addr_t>> AffinityMap;
AffinityMap _map;
}

void hpx_gas_set_affinity(hpx_addr_t addr, int worker) {
  DEBUG_IF(gas_owner_of(here->gas, addr) != here->rank) {
    dbg_error("Attempt to set affinity of %zu at %d (owned by %d)\n",
              addr, here->rank, gas_owner_of(here->gas, addr));
  }
  DEBUG_IF(worker < 0 || here->sched->n_workers <= worker) {
    dbg_error("Attempt to set affinity of %zu to %d is outside range [0, %d)\n",
              addr, worker, here->sched->n_workers);
  }
  // @todo: Should we be pinning addr? The interface doesn't require it, but it
  //        could prevent usage errors in AGAS? On the other hand, it could
  //        result in debugging issues with pin reference counting.
  _map.insert(addr, worker);
}

void hpx_gas_clear_affinity(hpx_addr_t addr) {
  DEBUG_IF(gas_owner_of(here->gas, addr) != here->rank) {
    dbg_error("Attempt to clear affinity of %zu at %d (owned by %d)\n",
              addr, here->rank, gas_owner_of(here->gas, addr));
  }
  _map.erase(addr);
}
