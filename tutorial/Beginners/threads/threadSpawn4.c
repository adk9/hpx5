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
// Example code - Thread Creation using hpx_parcel_send and Termination
//****************************************************************************
#include <stdio.h>
#include <hpx/hpx.h>

#define NUM_THREADS                5
static  hpx_action_t _main       = 0;
static  hpx_action_t _printHello = 0;

static int _printHello_action(int *args) {
  int tid = *args;
  printf("Hello World! It's me, thread #%d!\n", tid);
  return HPX_SUCCESS;
}

//****************************************************************************
// @Action which spawns the threads
//****************************************************************************
static int _main_action(void *args) {
  hpx_addr_t and = hpx_lco_and_new(NUM_THREADS);
  for (int i = 0; i < NUM_THREADS; i++) {
    hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(i));
    hpx_parcel_set_action(p, _printHello);
    hpx_parcel_set_target(p, HPX_HERE);
    hpx_parcel_set_cont_action(p, hpx_lco_set_action);
    hpx_parcel_set_cont_target(p, and);
    hpx_parcel_set_data(p, &i, sizeof(i));
    hpx_parcel_send_sync(p);
  }
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
   
  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _main, _main_action, HPX_SIZE_T, HPX_POINTER);
  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _printHello, _printHello_action, HPX_SIZE_T, HPX_POINTER);

  return hpx_run(&_main, NULL, 0);
}
