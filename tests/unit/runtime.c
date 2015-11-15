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

#define RUNS 3
#define ACTIONS_PER_RUN 10

int _foo_action_handler(void) {
  printf("Foo action : %u : ranks : %u : threads : %u.\n",
         hpx_get_my_rank(), hpx_get_num_ranks(), hpx_get_num_threads());
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, 0, _foo, _foo_action_handler);

int _main_action_handler(void) {
  printf("Hello World from main action rank : %u : ranks : %u : threads : %u.\n",
         hpx_get_my_rank(), hpx_get_num_ranks(), hpx_get_num_threads());

  hpx_addr_t done = hpx_lco_and_new(ACTIONS_PER_RUN); 
  for (int i = 0; i < ACTIONS_PER_RUN; i++) {
	int loc = i % hpx_get_num_ranks();  
  	hpx_call(HPX_THERE(loc), _foo, done);
  }
  hpx_lco_wait(done); 
  hpx_lco_delete(done, HPX_NULL);
  hpx_exit(HPX_SUCCESS);
}
HPX_ACTION(HPX_DEFAULT, 0, _hello, _main_action_handler); 

int main(int argc, char *argv[argc]) {
  if (hpx_init(&argc, &argv) != 0) {
    fprintf(stderr, "failed to initialize HPX.\n");
    return -1;
  }
  int success;
  for (int i = 0; i < RUNS; ++i) {
    success = hpx_run(&_hello);
    printf("%i hpx_run returned %d.\n", i+1, success);
  }

  hpx_finalize();
  printf("hpx_finalize completed %d.\n", 1);
  return success;
}
