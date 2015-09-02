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
// Example code -  hpx_thread_exit(int status)  -- Finish the current thread's
//                 execution. The behavior of this call depends on the status
//                 parameter, and is equivalent to returning status from the
//                 action.
// Possible status codes:
//                 HPX_SUCCESS: Normal termination, send a parcel with 0-sized 
//                              data to the thread's continuation address.
//                 HPX_ERROR: Abnormal termination. Terminates execution.
//                 HPX_RESEND: Terminate execution, and resend the thread's 
//                             parcel (NOT the continuation parcel). This
//                             can be used for application-level forwarding 
//                             when hpx_addr_try_pin() fails.
//                 HPX_LCO_EXCEPTION: Continue an exception to the
//                             continuation address.
//****************************************************************************
#include <stdio.h>
#include <assert.h>
#include <hpx/hpx.h>

static hpx_action_t _main       = 0;
static hpx_action_t _exitSuccess = 0;

static int _exitSuccess_action(int *args, size_t size) {
  hpx_thread_exit(HPX_SUCCESS);
}

//****************************************************************************
// @Action which spawns the threads
//****************************************************************************
static int _main_action(int *args, size_t size) {
  int value;

  hpx_addr_t done = hpx_lco_future_new(sizeof(int));
  hpx_status_t status = hpx_call(HPX_HERE, _exitSuccess, done, NULL, 0);
  assert(status == HPX_SUCCESS);

  hpx_lco_wait(done);
  hpx_lco_get(done, sizeof(int), &value);
  assert(value == HPX_SUCCESS);
  printf("Thread exited with HPX_SUCCESS #%d\n", value);

  hpx_lco_delete(done, HPX_NULL);

  hpx_shutdown(HPX_SUCCESS);
}

int main(int argc, char *argv[]) {
  int e = hpx_init(&argc, &argv);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }
   
  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _main, _main_action, HPX_POINTER, HPX_SIZE_T);
  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _exitSuccess, _exitSuccess_action, HPX_POINTER, HPX_SIZE_T);

  e = hpx_run(&_main, NULL, 0);
  hpx_finalize();
  return e;
}
