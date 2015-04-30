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

//****************************************************************************
// Example code - Thread continue cleanup tutorial -- Finishes the current
// thread's execution, sending value to the thread's contination address.
// This version gives the application to cleanup for instance, to free the 
// value. After dealing with the continued data, it will run cleanup(env)
//****************************************************************************
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <hpx/hpx.h>

static  hpx_action_t _main       = 0;
static  hpx_action_t _cleanup    = 0;

const int DATA_SIZE              = sizeof(uint64_t);
const int SET_CONT_VALUE         = 1234;

static int _cleanup_action(size_t size, void *args) {
  uint64_t *value = (uint64_t*) malloc(sizeof(uint64_t));
  *value = SET_CONT_VALUE;
  hpx_thread_continue_cleanup(DATA_SIZE, value, free, value);
}

//****************************************************************************
// @Action which spawns the thread
//****************************************************************************
static int _main_action(size_t size, int *args) {
  int rank = hpx_get_my_rank();
  uint64_t *block = malloc(DATA_SIZE);

  hpx_call_sync(HPX_HERE, _cleanup, block, DATA_SIZE, &rank, sizeof(rank));
  printf("Value in block is %"PRIu64"\n", *block);

  hpx_shutdown(HPX_SUCCESS);
}

int main(int argc, char *argv[]) {
  int e = hpx_init(&argc, &argv);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }
   
  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _main, _main_action, HPX_SIZE_T, HPX_POINTER);
  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _cleanup, _cleanup_action, HPX_SIZE_T, HPX_POINTER);

  return hpx_run(&_main, NULL, 0);
}
