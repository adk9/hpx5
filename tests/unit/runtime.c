// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2016, Trustees of Indiana University,
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

#define RUNS 1
#define ACTIONS_PER_RUN 10

int _foo_action_handler(void) {
  printf("Foo action : %u : ranks : %u : threads : %u.\n",
         hpx_get_my_rank(), hpx_get_num_ranks(), hpx_get_num_threads());
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, 0, _foo, _foo_action_handler);

static int _inner(int iteration) {
  printf("Hello World from main action rank : %u : ranks : %u : "
         "threads : %u : iteration : %d.\n",
         hpx_get_my_rank(), hpx_get_num_ranks(), hpx_get_num_threads(),
         iteration);

  hpx_addr_t done = hpx_lco_and_new(ACTIONS_PER_RUN);
  for (int i = 0; i < ACTIONS_PER_RUN; i++) {
    int loc = i % hpx_get_num_ranks();
    hpx_call(HPX_THERE(loc), _foo, done);
  }
  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);

  return (iteration & 1) ? HPX_SUCCESS : HPX_ERROR;
}

static int _diffusion_handler(int iteration) {
  int status = _inner(iteration);
  hpx_exit(status);
}
HPX_ACTION(HPX_DEFAULT, 0, _diffusion, _diffusion_handler, HPX_INT);

static int _spmd_handler(int iteration) {
  int status = _inner(iteration);
  return hpx_thread_continue(&status);
}
HPX_ACTION(HPX_DEFAULT, 0, _spmd, _spmd_handler, HPX_INT);

int main(int argc, char *argv[argc]) {
  if (hpx_init(&argc, &argv) != 0) {
    fprintf(stderr, "failed to initialize HPX.\n");
    return -1;
  }

  for (int i = 0; i < RUNS; ++i) {
    int success = hpx_run(&_diffusion, &i);
    printf("%i hpx_run returned %d.\n", i+1, success);
  }

  for (int i = 0; i < RUNS; ++i) {
    int success = hpx_run_spmd(&_spmd, &i);
    printf("%i hpx_run_spmd returned %d.\n", i+1, success);
  }

  hpx_finalize();
  printf("hpx_finalize completed %d.\n", 1);
  return 0;
}
