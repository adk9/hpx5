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
#include <inttypes.h>
#include <hpx/hpx.h>
#include <hpx/topology.h>
#include <libhpx/libhpx.h>
#include "tests.h"

static int _set_action(hpx_addr_t args, uint64_t count) {
  printf("Setting the lco in the set handler\n");
  assert(args);
  for(int i = 0; i < count; i++) {
    hpx_lco_and_set(args, HPX_NULL);
    printf("And lco set successfully\n");
}
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, HPX_COALESCED, _set, _set_action, HPX_ADDR, HPX_UINT64);

static int _checkcoalescing_action(void) {
  libhpx_config_t *cfg = libhpx_get_config();
  hpx_addr_t lco = hpx_lco_and_new(hpx_get_num_ranks());
  hpx_addr_t done = hpx_lco_future_new(0);
  uint64_t i = 1;
  //hpx_call(HPX_HERE, _set, done, &lco, sizeof(lco));
  hpx_bcast(_set, HPX_NULL, done, &lco, &i);

  hpx_lco_wait(lco);
  hpx_lco_wait(done);

  hpx_lco_delete(done, HPX_NULL);
  hpx_lco_delete(lco, HPX_NULL);

  printf("starting second test\n");
  lco = hpx_lco_and_new(cfg->coalescing_buffersize * hpx_get_num_ranks() + hpx_get_num_ranks());
  done = hpx_lco_future_new(0);
  i = (cfg->coalescing_buffersize * hpx_get_num_ranks() + hpx_get_num_ranks()) / HPX_LOCALITIES;
  //hpx_call(HPX_HERE, _set, done, &lco, sizeof(lco));
  hpx_bcast(_set, HPX_NULL, done, &lco, &i);

  hpx_lco_wait(lco);
  hpx_lco_wait(done);

  hpx_lco_delete(done, HPX_NULL);
  hpx_lco_delete(lco, HPX_NULL);


  printf("LCO Set succeeded\n");

  return HPX_SUCCESS;
  //hpx_exit(HPX_SUCCESS);
}

static HPX_ACTION(HPX_DEFAULT, 0, _checkcoalescing, _checkcoalescing_action);


TEST_MAIN({
  ADD_TEST(_checkcoalescing, 0);
});
