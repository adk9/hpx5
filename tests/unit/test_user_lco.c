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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "hpx/hpx.h"
#include "tests.h"

// Goal of this testcase is to use the user-defined LCO to achieve 
// the “OR” gate

static void _initBool (bool *input, const size_t size) {
  *input = 0;
}

// Update *lhs with the or gate value
static void _orBool (bool *lhs, const bool *rhs, const size_t size) {
  if ((*lhs == 0) && (*rhs == 0))
    *lhs = 0;
  else if ((*lhs == 1) && (*rhs == 0)) 
    *lhs = 1;
  else if ((*lhs == 0) && (*rhs == 1))
    *lhs = 1;
  else if ((*lhs == 1) && (*rhs == 1)) 
    *lhs = 1;
}

// A predicate that "guards" the LCO.
// This has to return true as soon as soon as first one gets set to 
// true.
static bool _predicateBool(bool *lhs, const size_t size) {
  if (*lhs == 1) 
    return 1;
  else
    return 0;
}

static HPX_PINNED(_or_set, void *UNUSED) {
  hpx_addr_t addr = hpx_thread_current_target();
  hpx_lco_set(addr, 0, NULL, HPX_NULL, HPX_NULL);
  return HPX_SUCCESS;
}

static HPX_ACTION(test_libhpx_user_lco, void *UNUSED) {
  printf("Starting the array of futures test\n");

  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t lco = hpx_lco_user_new(sizeof(bool), (hpx_monoid_id_t)_initBool,
                                     (hpx_monoid_op_t)_orBool,
                                     (hpx_predicate_t)_predicateBool);
  hpx_call(lco, _or_set, HPX_NULL, NULL, 0);

  hpx_lco_wait(lco);
  hpx_lco_delete(lco, HPX_NULL);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  return HPX_SUCCESS;
}

TEST_MAIN({
 ADD_TEST(test_libhpx_user_lco);
});
