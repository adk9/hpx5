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

static HPX_ACTION(_bcast_untyped, void *UNUSED) {
  return HPX_SUCCESS;
}

static hpx_action_t _bcast_typed;
static int _bcast_typed_action(int i, float f, char c) {
  printf("Typed action: %d %f %c!\n", i, f, c);
  return HPX_SUCCESS;
}
static HPX_ACTION_DEF(DEFAULT, _bcast_typed_action, _bcast_typed,
                      HPX_INT, HPX_FLOAT, HPX_CHAR);


static HPX_ACTION(bcast, void *UNUSED) {
  printf("Test hpx_bcast (untyped)\n");
  hpx_addr_t lco = hpx_lco_future_new(0);
  hpx_bcast(_bcast_untyped, lco, NULL, 0);
  hpx_lco_wait(lco);
  hpx_lco_delete(lco, HPX_NULL);

  printf("Test hpx_bcast_sync (untyped)\n");
  hpx_bcast_sync(_bcast_untyped, NULL, 0);

  int i = 42;
  float f = 1.0;
  char c = 'a';

  printf("Test hpx_bcast (typed)\n");
  lco = hpx_lco_future_new(0);
  hpx_bcast(_bcast_typed, lco, &i, &f, &c);
  hpx_lco_wait(lco);
  hpx_lco_delete(lco, HPX_NULL);

  printf("Test hpx_bcast_sync (typed)\n");
  hpx_bcast_sync(_bcast_typed, &i, &f, &c);
  return HPX_SUCCESS;
} 

TEST_MAIN({
 ADD_TEST(bcast);
});
