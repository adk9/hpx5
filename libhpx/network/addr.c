// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "hpx/hpx.h"

bool
hpx_addr_eq(const hpx_addr_t lhs, const hpx_addr_t rhs) {
  return (lhs.rank == rhs.rank) && (lhs.local == rhs.local);
}


hpx_addr_t
hpx_addr_from_rank(int rank) {
  hpx_addr_t r = { NULL, rank };
  return r;
}


int
hpx_addr_to_rank(const hpx_addr_t addr) {
  return addr.rank;
}


bool
hpx_addr_try_pin(const hpx_addr_t addr, void **local) {
  if (local)
    *local = addr.local;

  if (addr.rank == -1)
    return true;

  if (addr.rank != hpx_get_my_rank())
    return false;

  return true;
}


void
hpx_addr_unpin(const hpx_addr_t addr) {
}
