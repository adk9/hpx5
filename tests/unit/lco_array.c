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

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "hpx/hpx.h"
#include "tests.h"

#define ARRAY_SIZE 8

// This testcase tests the hpx_lco_array_new API function which
// allocates a global array of LCOs.
static HPX_PINNED(_set_future_value, void *UNUSED, void *args) {
  int size = hpx_thread_current_args_size();
  hpx_addr_t addr = hpx_thread_current_target();
  hpx_lco_set(addr, size, args, HPX_NULL, HPX_NULL);
  return HPX_SUCCESS;  
}

static HPX_ACTION(lco_future_array, void *UNUSED) {
  printf("Starting the array of futures test\n");
  uint64_t value = 0;
  srand(time(NULL));
  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

  // allocate 8 futures with size of each future's value 
  hpx_addr_t base = hpx_lco_future_local_array_new(ARRAY_SIZE, 
                                      sizeof(uint64_t));
  for (int i = 0; i < ARRAY_SIZE; i++) {
    value = (rand() / (float)RAND_MAX) * 100;
    hpx_addr_t other = hpx_lco_array_at(base, i, sizeof(uint64_t));
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

static HPX_ACTION(lco_and_array, void *UNUSED) {
  printf("Starting the array of lco test\n");
  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t lcos = hpx_lco_and_local_array_new(ARRAY_SIZE, 0);
  for (int i = 0; i < ARRAY_SIZE; i++) {
    hpx_addr_t done = hpx_lco_future_new(0);
    hpx_addr_t other = hpx_lco_array_at(lcos, i, 0);
    hpx_call(other, _set, done, NULL, 0);
    hpx_lco_wait(done);
    hpx_lco_delete(done, HPX_NULL);  
  }

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  return HPX_SUCCESS;
}

/// Initialize a double zero.
static void _initDouble(double *input, const size_t bytes) {
  *input = 0;
}

/// Update *lhs with with the max(lhs, rhs);
static void _maxDouble(double *lhs, const double *rhs, const size_t bytes) {
  *lhs = (*lhs > *rhs) ? *lhs : *rhs;
}

static HPX_ACTION(lco_reduce_array, void *UNUSED) {
  printf("Starting the array of reduce test\n");
  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t domain = hpx_gas_alloc_cyclic(ARRAY_SIZE, sizeof(double), sizeof(double));
  hpx_addr_t newdt = hpx_lco_reduce_local_array_new(ARRAY_SIZE, 
                                       ARRAY_SIZE, sizeof(double),
                                       (hpx_monoid_id_t)_initDouble,
                                       (hpx_monoid_op_t)_maxDouble);

  for (int i = 0; i < ARRAY_SIZE; i++) {
    hpx_addr_t other = hpx_lco_array_at(newdt, i, sizeof(double));
    // Compute my gnewdt, and then start the reduce
    for (int j = 0; j < ARRAY_SIZE; j++) {
      double gnewdt = 3.14*(j+1);
      hpx_lco_set(other, sizeof(double), &gnewdt, HPX_NULL, HPX_NULL);
      //printf("Reduce LCO Value set = %g\n", gnewdt);
    }
  }

  // Get the gathered value and print the debugging string.
  for (int i = 0; i < ARRAY_SIZE; i++) {
    double ans;
    hpx_addr_t other = hpx_lco_array_at(newdt, i, sizeof(double));
    hpx_lco_get(other, sizeof(double), &ans);
    printf("Reduce LCO Value got = %g\n", ans);
    double comp_value = 3.14*(ARRAY_SIZE);
    assert(fabs(ans - comp_value)/(fabs(ans) + fabs(comp_value)) < 0.001);
  }
  hpx_lco_delete(newdt, HPX_NULL);
  hpx_gas_free(domain, HPX_NULL);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  return HPX_SUCCESS;
}

TEST_MAIN({
 ADD_TEST(lco_future_array);
 ADD_TEST(lco_and_array);
 ADD_TEST(lco_reduce_array);
});
