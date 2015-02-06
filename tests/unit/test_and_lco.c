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

#include "hpx/hpx.h"
#include "tests.h"

static HPX_ACTION(_and_set, hpx_addr_t *args) {
  hpx_lco_and_set(*args, HPX_NULL);
  return HPX_SUCCESS;
}

static HPX_ACTION(test_libhpx_lco_and, void *UNUSED) {
  // Allocate an and LCO. This is synchronous. An and LCO generates an AND
  // gate. Inputs should be >=0;
  hpx_addr_t lco = hpx_lco_and_new(1);
  hpx_call(HPX_HERE, _and_set, HPX_NULL, &lco, sizeof(lco));
  hpx_lco_wait(lco);
  hpx_lco_delete(lco, HPX_NULL);
  return HPX_SUCCESS;
} 

TEST_MAIN({
 ADD_TEST(test_libhpx_lco_and);
});
