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

static  hpx_action_t _main = 0;
static  hpx_action_t _lcoSetGet  = 0;

static int _lcoSetGet_action(void *args) {
  uint64_t val = 1234, setVal;
  hpx_addr_t future = hpx_lco_future_new(sizeof(uint64_t));
  hpx_lco_set(future, sizeof(uint64_t), &val, HPX_NULL, HPX_NULL);
  hpx_lco_get(future, sizeof(setVal), &setVal);
  hpx_lco_delete(future, HPX_NULL);
  hpx_thread_continue(sizeof(uint64_t), &setVal);
}

static int _main_action(void *args) {
  hpx_addr_t lco;
  uint64_t result;
  hpx_addr_t done = hpx_lco_future_new(sizeof(uint64_t));
  hpx_call(HPX_HERE, _lcoSetGet, done, &lco, sizeof(lco));
  hpx_lco_wait(done);

  hpx_lco_get(done, sizeof(uint64_t), &result);
  printf("LCO value got = %" PRIu64 "\n", result);
  
  hpx_lco_delete(done, HPX_NULL);
  hpx_shutdown(HPX_SUCCESS);
}

int main(int argc, char *argv[]) {
  int e = hpx_init(&argc, &argv);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }
   
  HPX_REGISTER_ACTION(_main_action, &_main);
  HPX_REGISTER_ACTION(_lcoSetGet_action, &_lcoSetGet);

  return hpx_run(&_main, NULL, 0);
}
