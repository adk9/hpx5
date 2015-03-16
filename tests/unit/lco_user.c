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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "hpx/hpx.h"
#include "tests.h"

// Goal of this testcase is to use the user-defined LCO to achieve 
// the “OR” gate

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

static HPX_ACTION(_lco_get, void *UNUSED) {
  hpx_addr_t addr = hpx_thread_current_target();
  hpx_lco_wait(addr);
  hpx_lco_reset_sync(addr);
  return HPX_SUCCESS;
}

static HPX_ACTION(_lco_set, int *i) {
  hpx_addr_t addr = hpx_thread_current_target();
  int val = (*i == 15) ? 1 : (0 == (rand() % 5));
  hpx_lco_set(addr, sizeof(val), &val, HPX_NULL, HPX_NULL);
  if (val == 0) {
    hpx_lco_error_sync(addr, HPX_LCO_ERROR);
  }
  return HPX_SUCCESS;
}

static HPX_ACTION(lco_user, void *UNUSED) {
  printf("Test user lco.\n");
  srand(time(NULL));
  hpx_addr_t lco;
  lco = hpx_lco_user_new(sizeof(bool),
                         (hpx_monoid_id_t)_lco_init,
                         (hpx_monoid_op_t)_lco_op,
                         (hpx_predicate_t)_lco_predicate);
  for (int i = 0; i < 16; ++i) {
    hpx_addr_t and = hpx_lco_and_new(2);
    hpx_call(lco, _lco_set, and, &i, sizeof(i));
    hpx_call(lco, _lco_get, and, NULL, 0);
    hpx_lco_wait(and);
    hpx_lco_delete(and, HPX_NULL);
    hpx_lco_reset_sync(lco);
  }
  hpx_lco_delete(lco, HPX_NULL);
  return HPX_SUCCESS;
}

TEST_MAIN({
 ADD_TEST(lco_user);
});
