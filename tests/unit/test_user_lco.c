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

typedef struct or_gate {
  bool latch;
  int  count;
} _or_gate_t;

static void _lco_init (bool *val, const size_t size) {
  *val = 0;
}

// Update *lhs with the or gate value
static void _lco_op (bool *val, const bool *new, const size_t size) {
  *val ^= *new;
}

// A predicate that "guards" the LCO.
// This has to return true as soon as soon as first one gets set to 
// true.
static bool _lco_predicate(bool *val, const size_t size) {
  assert(val);
  return (*val);
}

static HPX_ACTION(_lco_set, int *i) {
  hpx_addr_t addr = hpx_thread_current_target();
  int val = (*i == 16) ? 1 : (*i == (rand() % 15));
  hpx_lco_set(addr, sizeof(val), &val, HPX_NULL, HPX_NULL);
  return HPX_SUCCESS;
}

static HPX_ACTION(test_libhpx_user_lco, void *UNUSED) {
  printf("Test user lco.\n");
  hpx_addr_t lco[2];
  lco[0] = hpx_lco_user_new(sizeof(_or_gate_t),
                            (hpx_monoid_id_t)_lco_init,
                            (hpx_monoid_op_t)_lco_op,
                            (hpx_predicate_t)_lco_predicate);
  lco[1] = hpx_lco_and_new(16);
  for (int i = 0; i < 16; ++i) {
    hpx_call(lco[0], _lco_set, lco[1], &i, sizeof(i));
  }
  hpx_lco_wait_all(2, lco, NULL);
  hpx_lco_delete(lco[0], HPX_NULL);
  hpx_lco_delete(lco[1], HPX_NULL);
  return HPX_SUCCESS;
}

TEST_MAIN({
 ADD_TEST(test_libhpx_user_lco);
});
