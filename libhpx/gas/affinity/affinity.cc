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

#include <libhpx/gas.h>
#include "none.h"
#include "urcu_map.h"
#include "cuckoo_hash.h"

using libhpx::gas::Affinity;
using libhpx::gas::CuckooHash;
using libhpx::gas::None;
using libhpx::gas::URCUMap;

Affinity::~Affinity() {
}

void* affinity_new(const config_t *config) {
  switch (config->gas_affinity) {
   default:
   case (HPX_GAS_AFFINITY_NONE): return new None;
   case (HPX_GAS_AFFINITY_URCU): return new URCUMap;
   case (HPX_GAS_AFFINITY_CUCKOO): return new CuckooHash;
  }
}

void affinity_delete(void *obj) {
  delete static_cast<Affinity*>(obj);
}

void affinity_set(void *obj, hpx_addr_t gva, int worker) {
  static_cast<Affinity*>(obj)->set(gva, worker);
}

void affinity_clear(void *obj, hpx_addr_t gva) {
  static_cast<Affinity*>(obj)->clear(gva);
}

int affinity_get(const void *obj, hpx_addr_t gva) {
  return static_cast<const Affinity*>(obj)->get(gva);
}
