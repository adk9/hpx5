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

static HPX_ACTION(_and_set, void *UNUSED) {
  return HPX_SUCCESS;
}

static HPX_ACTION(lco_and, void *UNUSED) {
  printf("Test hpx_lco_and\n");
  hpx_addr_t lco = hpx_lco_and_new(8);
  for (int i = 0; i < 8; ++i) {
    hpx_call(HPX_HERE, _and_set, lco, NULL, 0);
  }
  hpx_lco_wait(lco);
  hpx_lco_delete(lco, HPX_NULL);
  return HPX_SUCCESS;
}

static HPX_ACTION(_and_set_num, hpx_addr_t *lco) {
  hpx_lco_and_set_num(*lco, 4, HPX_NULL);
  return HPX_SUCCESS;
}

static HPX_ACTION(lco_and_num, void *UNUSED) {
  printf("Test hpx_lco_and_set_num\n");
  hpx_addr_t lco = hpx_lco_and_new(8);
  for (int i = 0; i < 4; ++i) {
    hpx_call(HPX_HERE, _and_set, lco, NULL, 0);
  }
  hpx_call(HPX_HERE, _and_set_num, HPX_NULL, &lco, sizeof(lco));
  hpx_lco_wait(lco);
  hpx_lco_delete(lco, HPX_NULL);
  return HPX_SUCCESS;
} 

TEST_MAIN({
 ADD_TEST(lco_and);
 ADD_TEST(lco_and_num);
});
