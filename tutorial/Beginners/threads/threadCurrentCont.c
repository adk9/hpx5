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
// Example code - Thread Continue functions -- hpx_thread_current_cont_target
//                Gets the address of the continuation for the current thread.
//                hpx_thread_current_cont_action -- Get the continuation 
//                action for the current thread.
//****************************************************************************
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>
#include <hpx/hpx.h>

static  hpx_action_t _main       = 0;
static  hpx_action_t _threadCont = 0;

const int DATA_SIZE = sizeof(uint64_t);
const int SET_CONT_VALUE = 1234;

static int _threadCont_action(void *args) {
  hpx_addr_t addr = hpx_thread_current_target();
  uint64_t local;
  if (!hpx_gas_try_pin(addr, (void**)&local))
    return HPX_RESEND;

  hpx_action_t c_action = hpx_thread_current_cont_action();
  hpx_addr_t   c_target = hpx_thread_current_cont_target();

  local = SET_CONT_VALUE;

  hpx_addr_t completed = hpx_lco_future_new(0);
  hpx_call(c_target, c_action, &local, DATA_SIZE, completed);
  hpx_lco_wait(completed);
  hpx_lco_delete(completed, HPX_NULL);

  hpx_gas_unpin(addr);
  return HPX_SUCCESS;

}

static int _main_action(void *args) {
  hpx_addr_t done = hpx_lco_future_new(DATA_SIZE);

  hpx_parcel_t *p = hpx_parcel_acquire(NULL, DATA_SIZE);
  hpx_parcel_set_target(p, HPX_HERE);
  hpx_parcel_set_action(p, _threadCont);
  hpx_parcel_set_cont_target(p, done);
  hpx_parcel_set_cont_action(p, hpx_lco_set_action);
  hpx_parcel_send(p, HPX_NULL);

  hpx_lco_wait(done);

  uint64_t result;
  hpx_lco_get(done, DATA_SIZE, &result);
  printf("Received continuation with value %" PRIu64 "\n", result);
  assert(result == SET_CONT_VALUE);

  hpx_lco_delete(done, HPX_NULL);

  hpx_shutdown(HPX_SUCCESS);
}

int main(int argc, char *argv[]) {
  int e = hpx_init(&argc, &argv);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }
   
  HPX_REGISTER_ACTION(_main_action, &_main);
  HPX_REGISTER_ACTION(_threadCont_action, &_threadCont);

  return hpx_run(&_main, NULL, 0);
}
