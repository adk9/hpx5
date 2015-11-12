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
#include <hpx/hpx.h>

static  hpx_action_t _main = 0;
static  hpx_action_t _set  = 0;

static int _set_action(void *args, size_t size) {
  hpx_addr_t addr = *(hpx_addr_t*)args;
  hpx_lco_error(addr, HPX_LCO_ERROR, HPX_NULL);
  return HPX_SUCCESS;
}

static int _main_action(void *args, size_t size) {
  hpx_addr_t lco = hpx_lco_and_new(1);
  hpx_addr_t done = hpx_lco_future_new(0);
  hpx_call(HPX_HERE, _set, done, &lco, sizeof(lco));

  hpx_status_t status = hpx_lco_wait(lco);
  assert(status == HPX_LCO_ERROR);
  hpx_lco_wait(done);

  hpx_lco_delete(done, HPX_NULL);
  hpx_lco_delete(lco, HPX_NULL);

  printf("Propagate an error to an LCO succeeded\n");

  hpx_exit(HPX_SUCCESS);
}

int main(int argc, char *argv[]) {
  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _main, _main_action, HPX_POINTER, HPX_SIZE_T);
  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _set, _set_action, HPX_POINTER, HPX_SIZE_T);

  int e = hpx_init(&argc, &argv);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }

  e = hpx_run(&_main, NULL, 0);
  hpx_finalize();
  return e;
}
