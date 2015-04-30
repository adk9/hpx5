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
// Example code - Thread Creation using hpx_par_call_sync and Termination
//****************************************************************************
#include <stdio.h>
#include <hpx/hpx.h>

#define NUM_THREADS                5
static  hpx_action_t _main       = 0;
static  hpx_action_t _printHello = 0;

static int _printHello_action(int *args) {
  printf("Hello World! It's me, thread #%d!\n", hpx_thread_get_tls_id());
  hpx_thread_exit(HPX_SUCCESS);
}

//****************************************************************************
// @Action which spawns the threads
//****************************************************************************
static int _main_action(int *args) {
  hpx_par_call_sync(_printHello, 0, NUM_THREADS, 8, 1000, 0, NULL, 0, 0);
  hpx_shutdown(HPX_SUCCESS);
}

int main(int argc, char *argv[]) {
  int e = hpx_init(&argc, &argv);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }
   
  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _main, _main_action, HPX_SIZE_T, HPX_POINTER);
  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _printHello, _printHello_action, HPX_SIZE_T, HPX_POINTER);

  return hpx_run(&_main, NULL, 0);
}
