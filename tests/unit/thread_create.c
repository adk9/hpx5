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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include "hpx/hpx.h"
#include "tests.h"
#include "libhpx/locality.h"
#include "libsync/queues.h"

#define NUM_THREADS 5
#define ARRAY_SIZE 100
#define BUFFER_SIZE 128

const int DATA_SIZE = sizeof(uint64_t);
const int SET_CONT_VALUE = 1234;

typedef struct initBuffer {
  int  index;
  char message[BUFFER_SIZE];
} initBuffer_t;

static HPX_ACTION(_initData, const initBuffer_t *args) {
 // Get the target of the current thread. The target of the thread is the
 // destination that a parcel was sent to to spawn the current thread.
 // hpx_thread_current_target() returns the address of the thread's target
  hpx_addr_t local = hpx_thread_current_target();
  initBuffer_t *ld = NULL;
  if (!hpx_gas_try_pin(local, (void**)&ld))
    return HPX_RESEND;

  ld->index = args->index;
  strcpy(ld->message, args->message);

  //Get the size of the arguments passed to the current thread
  //uint32_t size = hpx_thread_current_args_size();

  hpx_gas_unpin(local);
  //printf("Initialized buffer with index: %u, with message: %s, size of arguments = %d\n", ld->index, ld->message, size);
  return HPX_SUCCESS;
}

// Test code -- ThreadCreate
static HPX_ACTION(thread_create, void *UNUSED) {
  printf("Starting the Threads test\n");
  // Start the timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t addr = hpx_gas_global_alloc(NUM_THREADS, sizeof(initBuffer_t));
  hpx_addr_t done = hpx_lco_and_new(NUM_THREADS);

  // HPX Threads are spawned as a result of hpx_parcel_send() / hpx_parcel_
  // sync().
  for (int t = 0; t < NUM_THREADS; t++) {
    hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(initBuffer_t));

    // Fill the buffer
    initBuffer_t *init = hpx_parcel_get_data(p);
    init->index = t;
    strcpy(init->message, "Thread creation test");

    // Set the target address and action for the parcel
    hpx_parcel_set_target(p, hpx_addr_add(addr, sizeof(initBuffer_t) * t, sizeof(initBuffer_t)));
    hpx_parcel_set_action(p, _initData);

    // Set the continuation target and action for parcel
    hpx_parcel_set_cont_target(p, done);
    hpx_parcel_set_cont_action(p, hpx_lco_set_action);

    // and send the parcel, this spawns the HPX thread
    hpx_parcel_send(p, HPX_NULL);
  }

  hpx_lco_wait(done);

  hpx_lco_delete(done, HPX_NULL);
  hpx_gas_free(addr, HPX_NULL);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  return HPX_SUCCESS;
}

// Finish the current thread's execution. The behavior of this call depends
// on the status parameter, and is equivalent to returning status from
// the action.
// HPX_SUCCESS: Normal termination, send a parcel with 0-sized data to the
// the thread's continuation address.
static HPX_ACTION(_worker, uint64_t *args) {
  //uint64_t n;
  //n = *(uint64_t*)args;

  //printf("Value of n =  %"PRIu64" \n", n);
  hpx_thread_exit(HPX_LCO_ERROR);
}

static HPX_ACTION(thread_exit, void *UNUSED) {
  printf("Starting the Thread Exit test\n");
  // Start the timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t done = hpx_lco_future_new(sizeof(uint64_t));
  uint64_t value = SET_CONT_VALUE;
  hpx_status_t status = hpx_call(HPX_HERE, _worker, done, &value, sizeof(value));
  assert_msg(status == HPX_SUCCESS, "Could not normally terminate the thread");
  hpx_lco_wait(done);

  hpx_lco_get(done, sizeof(uint64_t), &value);
  assert(value == HPX_SUCCESS);

  hpx_lco_delete(done, HPX_NULL);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  return HPX_SUCCESS;
}

TEST_MAIN({
  ADD_TEST(thread_create);
  ADD_TEST(thread_exit);
});
