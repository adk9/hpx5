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
#include <inttypes.h>
#include <hpx/hpx.h>

static  hpx_action_t _main = 0;
static  hpx_action_t _setValue  = 0;

static const int NUM_FUTURES = 5;

static int _setValue_action(void *args, size_t size) {
  // Generate the random value
  uint64_t  value = (rand() / (float)RAND_MAX) * 100;
  HPX_THREAD_CONTINUE(value);
}

static int _main_action(void *args, size_t size) {
  uint64_t values[NUM_FUTURES];
  void *addrs[NUM_FUTURES];
  int sizes[NUM_FUTURES];
  hpx_addr_t futures[NUM_FUTURES];

  for (int i = 0; i < NUM_FUTURES; i++) {
    addrs[i] = &values[i];
    sizes[i] = sizeof(uint64_t);
    futures[i] = hpx_lco_future_new(sizeof(uint64_t));
    hpx_call(HPX_HERE, _setValue, futures[i], NULL, 0);
  }

  hpx_lco_get_all(NUM_FUTURES, futures, sizes, addrs, NULL);
  hpx_lco_wait_all(NUM_FUTURES, futures, NULL);

  for (int i = 0; i < NUM_FUTURES; i++) {
    printf("values[%d] = %"PRIu64"\n", i, values[i]);
    hpx_lco_delete(futures[i], HPX_NULL);
  }

  hpx_exit(HPX_SUCCESS);
}

int main(int argc, char *argv[]) {
  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _main, _main_action, HPX_POINTER, HPX_SIZE_T);
  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _setValue, _setValue_action, HPX_POINTER, HPX_SIZE_T);

  // Seed the random number generator to get different results each time.
  srand(time(NULL));

  int e = hpx_init(&argc, &argv);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }

  e = hpx_run(&_main, NULL, 0);
  hpx_finalize();
  return e;
}
