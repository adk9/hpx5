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
#include <math.h>
#include <time.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include "tests.h"

static int _reduce_handler(hpx_addr_t args) {
  printf("Setting the lco in the reduce one handler\n");
  hpx_lco_and_set(args, HPX_NULL);
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, HPX_COALESCED, _reduce, _reduce_handler, HPX_ADDR);

static int lco_reduce_handler(void) {
  printf("In lco reduce one handler\n");
  hpx_addr_t and = hpx_lco_and_new(here->ranks);
  hpx_bcast(_reduce, HPX_NULL, HPX_NULL, &and);
  hpx_lco_wait(and);
  hpx_lco_delete(and, HPX_NULL);

  /* hpx_addr_t and2 = hpx_lco_and_new(here->config->coalescing_buffersize); */
  /* hpx_bcast_rsync(_reduce, &and2, sizeof(and2)); */
  /* hpx_lco_wait(and2); */
  /* hpx_lco_delete(and2, HPX_NULL); */

  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, 0, lco_reduce, lco_reduce_handler);


TEST_MAIN({
  ADD_TEST(lco_reduce, 0);
});
