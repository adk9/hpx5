//****************************************************************************
// @Filename      06_TestFuture.c
// @Project       High Performance ParallelX Library (libhpx)
//----------------------------------------------------------------------------
// @Subject       Library Unit Test Harness - Memory Management
//
// @Compiler      GCC
// @OS            Linux
// @Description   future.h File Reference
// @Goal          Goal of this testcase is to test future LCO
// @Copyright     Copyright (c) 2014, Trustees of Indiana University
//                All rights reserved.
//
//                This software may be modified and distributed under the terms
//                of the BSD license.  See the COPYING file for details.
//
//                This software was created at the Indiana University Center
//                for Research in Extreme Scale Technologies (CREST).
//----------------------------------------------------------------------------
// @Date          08/26/2014
// @Author        Jayashree Candadai <jayaajay [at] indiana.edu>
// @Version       0.1
// Commands to Run: make, mpirun hpxtest
//****************************************************************************

//****************************************************************************
// @Project Includes
//****************************************************************************
#include "hpx/hpx.h"
#include "tests.h"

#define SET_VALUE 1234
static uint64_t value;

//****************************************************************************
// hpx_lco_future_new test: This tests creation of future. Futures are built
// -in LCO's that represent values returned from async computation. Futures
// are always allocated in the global address space, because their addresses
// are used as the targets of parcels.
//****************************************************************************
int t06_get_value_action(void *args) {
  HPX_THREAD_CONTINUE(value);
}

int t06_set_value_action(void *args) {
  value = *(uint64_t*)args;
  //printf("At rank %d received value %"PRIu64" \n", hpx_get_my_rank(), value);
  return HPX_SUCCESS;
}

START_TEST (test_libhpx_lco_future_new)
{
  int count;
  fprintf(test_log, "Starting the Future LCO test\n");
  if(HPX_LOCALITIES < 2)
    count = 2;
  else
    count = HPX_LOCALITIES;

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
    hpx_call(HPX_THERE(i), t06_get_value, futures[i], NULL, 0);
  }

  hpx_lco_get_all(count, futures, sizes, addresses, NULL);

  value = SET_VALUE;

  for (int i = 0; i < count; i++) {
    hpx_lco_delete(futures[i], HPX_NULL);
    futures[i] = hpx_lco_future_new(0);
    hpx_call(HPX_THERE(i), t06_set_value, futures[i], &value, sizeof(value));
  }

  hpx_lco_get_all(count, futures, sizes, addresses, NULL);

  for (int i = 0; i < count; i++)
    hpx_lco_delete(futures[i], HPX_NULL);

  fprintf(test_log, " Elapsed: %g\n", hpx_time_elapsed_ms(t1));
}
END_TEST

//****************************************************************************
// This testcase tests the hpx_lco_future_array_new API function which
// allocates a global array of futures and hpx_lco_future_array_at which gets
// an address of a future in a future array
//****************************************************************************
int t06_get_future_value_action(void *args) {
  uint64_t data = SET_VALUE;
  HPX_THREAD_CONTINUE(data);
}

START_TEST (test_libhpx_lco_future_array)
{
  fprintf(test_log, "Starting the array of futures test\n");
  uint64_t value = 0;
  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

  // allocate 2 futures with size of each future's value and the
  // one future per block
  hpx_addr_t base = hpx_lco_future_array_new(2, sizeof(uint64_t), 1);
  hpx_addr_t other = hpx_lco_future_array_at(base, 1, sizeof(uint64_t), 1);

  hpx_call_sync(other, t06_get_future_value, &value, sizeof(value), NULL, 0);
  fprintf(test_log, "value = %"PRIu64"\n", value);

  fprintf(test_log, " Elapsed: %g\n", hpx_time_elapsed_ms(t1));
}
END_TEST

//****************************************************************************
// Register tests from this file
//****************************************************************************

void add_06_TestFutures(TCase *tc) {
  tcase_add_test(tc, test_libhpx_lco_future_new);
  tcase_add_test(tc, test_libhpx_lco_future_array);
}
