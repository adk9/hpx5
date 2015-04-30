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
// Example code - Thread continue tutorial - Finish the current thread's
// execution, sending value to the thread's continuation address.
//****************************************************************************
#include <stdio.h>
#include <unistd.h>
#include <hpx/hpx.h>

#define NUM_THREADS                    2
static  hpx_action_t _main           = 0;
static  hpx_action_t _doSomething    = 0;

static int _doSomething_action(int *args) {
  int ret1, ret2;
  int tid = *args;

  // Do some work
  sleep(1);

  if (tid == 0) {
    printf("First thread processing done\n");
    ret1 = 100;
    hpx_thread_continue(sizeof(ret1), &ret1);
  } else {
    printf("Second thread processing done\n");
    ret2 = 200;
    hpx_thread_continue(sizeof(ret2), &ret2);
  }

  return HPX_SUCCESS;
}

//****************************************************************************
// @Action which spawns the threads
//****************************************************************************
static int _main_action(void *args) {
  int value[] = {0, 0};
  void *addrs[] = {&value[0], &value[1]};
  int sizes[] = {sizeof(int), sizeof(int)};

  hpx_addr_t futures[] = {
    hpx_lco_future_new(sizeof(int)),
    hpx_lco_future_new(sizeof(int))
  };

  for (int i = 0; i < NUM_THREADS; i++)
    hpx_call(HPX_HERE, _doSomething, futures[i], &i, sizeof(i));

  hpx_lco_get_all(2, futures, sizes, addrs, NULL);
  hpx_lco_delete(futures[0], HPX_NULL);
  hpx_lco_delete(futures[1], HPX_NULL);

  printf("Return value from first thread is [%d]\n", value[0]);
  printf("Return value from second thread is [%d]\n", value[1]);

  hpx_shutdown(HPX_SUCCESS);
}

int main(int argc, char *argv[]) {
  int e = hpx_init(&argc, &argv);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }

  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _main, _main_action, HPX_SIZE_T, HPX_POINTER);
  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _doSomething, _doSomething_action, HPX_SIZE_T, HPX_POINTER);

  return hpx_run(&_main, NULL, 0);
}
