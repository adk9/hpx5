// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
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

#include <string.h>
#include <libhpx/locality.h>
#include <libhpx/network.h>
#include "agas.h"
#include "btt.h"

void
agas_move(void *gas, hpx_addr_t src, hpx_addr_t dst, hpx_addr_t sync) {
}

int
agas_memput(void *gas, hpx_addr_t to, const void *from, size_t n,
            hpx_addr_t lsync, hpx_addr_t rsync) {
  return HPX_ERROR;
}

int
agas_memget(void *gas, void *to, hpx_addr_t from, size_t n, hpx_addr_t lsync) {
  if (!n) {
    return HPX_SUCCESS;
  }

  agas_t *agas = gas;
  gva_t gva = { .addr = from };
  void *lfrom = NULL;
  if (btt_try_pin(agas->btt, gva, &lfrom)) {
    memcpy(to, lfrom, n);
    btt_unpin(agas->btt, gva);
    hpx_lco_set(lsync, 0, NULL, HPX_NULL, HPX_NULL);
    return HPX_SUCCESS;
  }
  else {
    return network_get(here->network, to, from, n, lco_set, lsync);
  }
  return HPX_SUCCESS;
}

int
agas_memcpy(void *gas, hpx_addr_t to, hpx_addr_t from, size_t size,
            hpx_addr_t sync) {
  return HPX_ERROR;
}
