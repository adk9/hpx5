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

#include "hpx/hpx.h"
#include "tests.h"

static HPX_ACTION(_and_set, hpx_addr_t *lco) {
  hpx_lco_set(*lco, 0, NULL, HPX_NULL, HPX_NULL);
  return HPX_SUCCESS;
}

static HPX_ACTION(_and_wait, hpx_addr_t *lco) {
  hpx_addr_t sync = hpx_lco_future_new(0);
  hpx_lco_wait(*lco);
  hpx_lco_delete(*lco, sync);
  hpx_lco_wait(sync);
  hpx_lco_delete(sync, HPX_NULL);
  return HPX_SUCCESS;
}

static HPX_ACTION(lco_and_set_fail, void *UNUSED) {
  hpx_addr_t lco = hpx_lco_and_new(8);
  for (int i = 0; i < 16; ++i) {
    hpx_call_sync(HPX_HERE, _and_set, NULL, 0, &lco, sizeof(lco));
    if (i == 7) {
      hpx_call_sync(HPX_HERE, _and_wait, NULL, 0, &lco, sizeof(lco));
    }
  }
  return HPX_SUCCESS;
}

TEST_MAIN({
 ADD_TEST(lco_and_set_fail);
});
