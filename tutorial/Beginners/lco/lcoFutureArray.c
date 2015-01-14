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
#include <inttypes.h>
#include <hpx/hpx.h>

static  hpx_action_t _main = 0;
static  hpx_action_t _get  = 0;

static int _get_action(void *args) {
  uint64_t data = 1234;
  HPX_THREAD_CONTINUE(data);
}

static int _main_action(void *args) {
  uint64_t value = 0;

  // allocate 2 futures with size of each future's value and the
  // one future per block
  hpx_addr_t base = hpx_lco_future_array_new(2, sizeof(uint64_t), 1);
  hpx_addr_t other = hpx_lco_future_array_at(base, 1, sizeof(uint64_t), 1);

  hpx_call_sync(other, _get, NULL, 0, &value, sizeof(value));
  printf("value = %"PRIu64"\n", value);

  hpx_shutdown(HPX_SUCCESS);
}

int main(int argc, char *argv[]) {
  int e = hpx_init(&argc, &argv);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }
   
  HPX_REGISTER_ACTION(_main_action, &_main);
  HPX_REGISTER_ACTION(_get_action, &_get);

  return hpx_run(&_main, NULL, 0);
}
