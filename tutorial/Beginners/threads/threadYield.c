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
// Example code - Thread Yield - Pause execution and gives other threads
// the opportunity to be scheduled
//****************************************************************************

#include <stdio.h>
#include <hpx/hpx.h> 

static hpx_action_t _main = 0;
static hpx_action_t _runMe = 0;

volatile int bStop = false;

struct thread_data {
  char *message;
};

static int _runMe_action(void *threadarg) {
  struct thread_data *my_data = (struct thread_data *)threadarg;
  char *message = my_data->message;

  while (!bStop) {
    printf("%s, ", (char *)message);
    hpx_thread_yield();
  }
  hpx_thread_exit(HPX_SUCCESS);
}

static int _main_action(void *args) {
  int count = 100;
  printf("Starting the thread yield test\n");

  char *messages[2] = {"PING", "PONG"};
  struct thread_data thread_args[2];

  hpx_addr_t and = hpx_lco_and_new(2);
  for (int i = 0; i < 2; i++) {
    thread_args[i].message = messages[i];
    hpx_call(HPX_HERE, _runMe, and, &thread_args[i], sizeof(thread_args));
  }

  do {
    count = count - 1;
    hpx_thread_yield();
  } while (count > 0);

  // Stop the threads;
  bStop = true;

  hpx_lco_wait(and);
  hpx_lco_delete(and, HPX_NULL);

  printf("\nDone!\n");

  hpx_shutdown(HPX_SUCCESS);
}

int main(int argc, char *argv[]) {
  int e = hpx_init(&argc, &argv);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }

  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _main, _main_action, HPX_SIZE_T, HPX_POINTER);
  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _runMe, _runMe_action, HPX_SIZE_T, HPX_POINTER);

  return hpx_run(&_main, NULL, 0);
}

