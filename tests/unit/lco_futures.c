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
#include "hpx/hpx.h"
#include "tests.h"

#define SET_VALUE 1234
static uint64_t value;

// hpx_lco_future_new test: This tests creation of future. Futures are built in
// in LCOs that represent values returned from async computation. Futures
// are always allocated in the global address space, because their addresses
// are used as the targets of parcels.
static HPX_ACTION(_get_value, void *args) {
  HPX_THREAD_CONTINUE(value);
}

static HPX_ACTION(_set_value, uint64_t *args) {
  value = *args;
  return HPX_SUCCESS;
}

static HPX_ACTION(lco_future_new, void *UNUSED) {
  printf("Starting the Future LCO test\n");

  int count = HPX_LOCALITIES;
  uint64_t values[count];
  void *addresses[count];
  int sizes[count];
  hpx_addr_t futures[count];

  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

  for (int i = 0; i < count; i++) {
    addresses[i] = &values[i];
    sizes[i] = sizeof(uint64_t);
    futures[i] = hpx_lco_future_new(sizeof(uint64_t));
    hpx_call(HPX_THERE(i), _get_value, futures[i], NULL, 0);
  }

  hpx_lco_get_all(count, futures, sizes, addresses, NULL);

  value = SET_VALUE;

  for (int i = 0; i < count; i++) {
    hpx_lco_delete(futures[i], HPX_NULL);
    futures[i] = hpx_lco_future_new(0);
    hpx_call(HPX_THERE(i), _set_value, futures[i], &value, sizeof(value));
  }

  hpx_lco_get_all(count, futures, sizes, addresses, NULL);

  for (int i = 0; i < count; i++) {
    hpx_lco_delete(futures[i], HPX_NULL);
  }

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  return HPX_SUCCESS;
}

// This testcase tests the hpx_lco_future_array_new API function which
// allocates a global array of futures and hpx_lco_future_array_at which gets
// an address of a future in a future array
static HPX_ACTION(_get_future_value, void *args) {
  uint64_t data = SET_VALUE;
  HPX_THREAD_CONTINUE(data);
}

static HPX_ACTION(lco_future_array, void *UNUSED) {
  printf("Starting the array of futures test\n");
  uint64_t value = 0;
  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

  // allocate 2 futures with size of each future's value and the
  // one future per block
  hpx_addr_t base = hpx_lco_future_array_new(2, sizeof(uint64_t), 1);
  hpx_addr_t other = hpx_lco_future_array_at(base, 1, sizeof(uint64_t), 1);

  hpx_call_sync(other, _get_future_value, &value, sizeof(value), NULL, 0);
  printf("value = %"PRIu64"\n", value);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  return HPX_SUCCESS;
}

TEST_MAIN({
 ADD_TEST(lco_future_new);
 ADD_TEST(lco_future_array);
});
