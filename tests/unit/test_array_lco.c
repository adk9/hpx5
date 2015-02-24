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

#include <inttypes.h>
#include <stdlib.h>
#include "hpx/hpx.h"
#include "tests.h"

// This testcase tests the hpx_lco_array_new API function which
// allocates a global array of LCOs.
static HPX_PINNED(_set_future_value, void *args) {
  int size = hpx_thread_current_args_size();
  hpx_addr_t addr = hpx_thread_current_target();
  hpx_lco_set(addr, size, args, HPX_NULL, HPX_NULL);
  return HPX_SUCCESS;  
}

static HPX_ACTION(test_libhpx_lco_future_array, void *UNUSED) {
  int array_size = 8;
  printf("Starting the array of futures test\n");
  uint64_t value = 0;
  srand(time(NULL));
  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

  // allocate 8 futures with size of each future's value 
  hpx_addr_t base = hpx_lco_array_new(HPX_TYPE_FUTURE, array_size, 
                                      sizeof(uint64_t));
  for (int i = 0; i < array_size; i++) {
    value = (rand() / (float)RAND_MAX) * 100;
    hpx_addr_t other = hpx_lco_array_at(HPX_TYPE_FUTURE, base, i, 
                                        sizeof(uint64_t));
    hpx_call_sync(other, _set_future_value, NULL, 0, &value, sizeof(value));

    uint64_t result;
    hpx_lco_get(other, sizeof(result), &result);

    if (result == value) {
      printf("Success! value of the future= %"PRIu64"\n", result);
    } else {
      printf("Failure! value of the "
             "future= %"PRIu64" but was set with %"PRIu64"\n",
             result, value);
    }
  }

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  return HPX_SUCCESS;
}

static HPX_ACTION(_set, void *args) {
  return HPX_SUCCESS;
}

static HPX_ACTION(test_libhpx_lco_and_array, void *UNUSED) {
  int array_size = 8;
  printf("Starting the array of lco test\n");
  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t lcos = hpx_lco_array_new(HPX_TYPE_AND, array_size, 0);
  for (int i = 0; i < array_size; i++) {
    hpx_addr_t done = hpx_lco_future_new(0);
    hpx_addr_t other = hpx_lco_array_at(HPX_TYPE_AND, lcos, i, 0);
    hpx_call(other, _set, done, NULL, 0);
    hpx_lco_wait(done);
    hpx_lco_delete(done, HPX_NULL);  
  }

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  return HPX_SUCCESS;
}

TEST_MAIN({
 ADD_TEST(test_libhpx_lco_future_array);
 ADD_TEST(test_libhpx_lco_and_array);
});
