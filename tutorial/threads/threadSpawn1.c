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

//****************************************************************************
// Example code - Thread Creation using hpx_call and Termination
//****************************************************************************
#include <stdio.h>
#include <hpx/hpx.h>

#define NUM_THREADS                5
static  hpx_action_t _main       = 0;
static  hpx_action_t _printHello = 0;

static int _printHello_action(int *args) {
  int tid = *args;
  printf("Hello World! It's me, thread #%d!\n", tid);
  hpx_thread_continue(0, NULL);
}

//****************************************************************************
// @Action which spawns the threads
//****************************************************************************
static int _main_action(void *args) {
  hpx_addr_t and = hpx_lco_and_new(NUM_THREADS);
  for (int i = 0; i < NUM_THREADS; i++) 
    hpx_call(HPX_HERE, _printHello, &i, sizeof(i), and);

  hpx_lco_wait(and);
  hpx_lco_delete(and, HPX_NULL);

  hpx_shutdown(HPX_SUCCESS);
}

int main(int argc, char *argv[]) {
  int e = hpx_init(&argc, &argv);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }
   
  HPX_REGISTER_ACTION(&_main, _main_action);
  HPX_REGISTER_ACTION(&_printHello, _printHello_action);

  return hpx_run(&_main, NULL, 0);
}
