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
#include "hpx/hpx.h"
#include "tests.h"

// This testcase tests the hpx_lco_type_array_new API function which
// allocates a global array of lcos
static HPX_ACTION(_get_future_value, void *args) {
  uint64_t data = 5678;
  HPX_THREAD_CONTINUE(data);
}

static HPX_ACTION(test_libhpx_lco_future_array, void *UNUSED) {
  printf("Starting the array of futures test\n");
  uint64_t value = 0;
  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

  // allocate 2 futures with size of each future's value 
  hpx_addr_t base = hpx_lco_type_array_new(HPX_TYPE_FUTURE, 2, sizeof(uint64_t));
  hpx_addr_t other = hpx_lco_type_array_at(HPX_TYPE_FUTURE, base, 1, 
                                           sizeof(uint64_t));

  hpx_call_sync(other, _get_future_value, &value, sizeof(value), NULL, 0);
  printf("value = %"PRIu64"\n", value);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  return HPX_SUCCESS;
}

static HPX_ACTION(_set, void *args) {
  uint64_t val = 1234, setVal;
  hpx_addr_t future = hpx_lco_future_new(sizeof(uint64_t));
  hpx_lco_set(future, sizeof(uint64_t), &val, HPX_NULL, HPX_NULL);
  hpx_lco_get(future, sizeof(setVal), &setVal);
  hpx_lco_delete(future, HPX_NULL);
  hpx_thread_continue(sizeof(uint64_t), &setVal);  
}

static HPX_ACTION(test_libhpx_lco_and_array, void *UNUSED) {
  printf("Starting the array of lco test\n");
  uint64_t result;
  hpx_time_t t1 = hpx_time_now();
  hpx_addr_t done = hpx_lco_future_new(sizeof(uint64_t));
  hpx_addr_t lcos = hpx_lco_type_array_new(HPX_TYPE_AND, 8);
  hpx_addr_t other = hpx_lco_type_array_at(HPX_TYPE_AND, lcos, 1);
  hpx_call(other, _set, done, NULL, 0);  

  hpx_lco_wait(done);
  hpx_lco_get(done, sizeof(uint64_t), &result);
  printf("LCO value got = %" PRIu64 "\n", result);

  hpx_lco_delete(done, HPX_NULL);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  return HPX_SUCCESS;
}

TEST_MAIN({
 ADD_TEST(test_libhpx_lco_future_array);
 ADD_TEST(test_libhpx_lco_and_array);
});
