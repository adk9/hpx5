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
// Example code - Thread set affinity test
//****************************************************************************
#include <stdio.h>
#include <hpx/hpx.h>

#define NUM_THREADS               4
static hpx_action_t _main       = 0;
static hpx_action_t _setID      = 0;

static void _setID_action(int *args)
{
  int tid = *args;

  printf("Thread #%d, Before: threadID (heavy, light) (#%d #%d), ProcessID = %d\n", 
          tid, HPX_THREAD_ID, hpx_thread_get_tls_id(), (int)hpx_thread_current_pid());
  hpx_thread_set_affinity(tid);
  printf("Thread #%d, After set affinity: threadID (heavy, light) (#%d #%d), ProcessID = %d\n", tid, HPX_THREAD_ID, hpx_thread_get_tls_id(), (int)hpx_thread_current_pid());

  hpx_thread_continue(0, NULL);
}

static int _main_action(void *args) {
  printf("Set the affinity of a lightweight thread\n");

  hpx_addr_t and = hpx_lco_and_new(NUM_THREADS);
  for (int i = 0; i < NUM_THREADS; i++) {
    hpx_call(HPX_HERE, _setID, &i, sizeof(i), and);
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

  HPX_REGISTER_ACTION(&_main, _main_action);
  HPX_REGISTER_ACTION(&_setID, _setID_action);

  return hpx_run(&_main, NULL, 0);
}
