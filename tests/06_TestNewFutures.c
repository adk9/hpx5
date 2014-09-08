//****************************************************************************
// @Filename      06_TestNewFutures.c
// @Project       High Performance ParallelX Library (libhpx)
//----------------------------------------------------------------------------
// @Subject       Library Unit Test Harness - futures
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

#define SET_VALUE 1234
static uint64_t value;

//****************************************************************************
// hpx_lco_newfuture_waitat for empty test: 
// This tests the creation of a single future. In the test, we wait on the
// future to see if it is empty, since a future should be empty on creation.
// Finally we free the future.
//****************************************************************************
int t06_waitforempty_action(hpx_addr_t *fut) {
  hpx_lco_newfuture_waitat(*future, 0, HPX_UNSET);
  return HPX_SUCCESS;
}

START_TEST (test_hpx_lco_newfuture_waitat_empty)
{
  printf("Starting the hpx_lco_newfuture_waitat() empty test\n");

  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t done = hpx_lco_future_new(0);
  hpx_addr_t fut = hpx_lco_newfuture_new(sizeof(uint64_t));
  hpx_call(HPX_HERE, t06_get_value, &fut, sizeof(future), done);
  hpx_lco_future_wait(done);
  
  hpx_lco_newfuture_free(fut);
  hpx_lco_future_free(done);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
} 
END_TEST

//****************************************************************************
// hpx_lco_newfuture_waitat for empty (remote version) test:
// Just like the hpx_lco_newfuture_waitat test but this time we wait
// remotely (if we have more than one locality)
//****************************************************************************
START_TEST (test_libhpx_lco_newfuture_waitat_empty_remote)
{
  if (hpx_get_num_localities() > 1) {

    printf("Starting the hpx_lco_newfuture_waitat() empty (remote) test\n");
    
    // allocate and start a timer
    hpx_time_t t1 = hpx_time_now();
    
    hpx_addr_t done = hpx_lco_future_new(0);
    hpx_addr_t fut = hpx_lco_newfuture_new(sizeof(uint64_t));
    hpx_call(HPX_THERE(1), t06_get_value, &fut, sizeof(future), done);
    hpx_lco_future_wait(done);
    
    hpx_lco_newfuture_free(fut);
    hpx_lco_future_free(done);

    printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  }
} 
END_TEST

//****************************************************************************
// hpx_lco_newfuture_waitat full test: 
// This tests the waiting for a future to be set. This test includes setting
// the future, but we do not actually test the value that gets set.
//****************************************************************************
int t06_waitforfull_action(hpx_addr_t *fut) {
  hpx_lco_newfuture_waitat(*future, 0, HPX_SET);
  return HPX_SUCCESS;
}

START_TEST (test_hpx_lco_newfuture_waitat_full)
{
  printf("Starting the hpx_lco_newfuture_waitat() full test\n");

  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t done = hpx_lco_future_new(0);
  hpx_addr_t lsync = hpx_lco_future_new(0);
  hpx_addr_t rsync = hpx_lco_future_new(0);
  hpx_addr_t fut = hpx_lco_newfuture_new(sizeof(uint64_t));
  hpx_call(HPX_HERE, t06_get_value, &fut, sizeof(future), done);
  hpx_lco_newfuture_setat(fut, 0, sizeof(SET_VALUE), SET_VALUE, lsync, rsync);
  hpx_lco_wait(lsync);
  hpx_lco_wait(rsync);
  hpx_lco_wait(done);
  
  hpx_lco_newfuture_free(fut);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
} 
END_TEST

//****************************************************************************
// hpx_lco_newfuture_waitat full remote test: 
// This tests the setting of a single future and waiting for a future to be set.
//****************************************************************************
START_TEST (test_hpx_lco_newfuture_waitat_full_remote)
{
  if (hpx_get_num_localities() > 1) {
    printf("Starting the hpx_lco_newfuture_waitat() full remote test\n");

    // allocate and start a timer
    hpx_time_t t1 = hpx_time_now();
    
    hpx_addr_t done = hpx_lco_future_new(0);
    hpx_addr_t lsync = hpx_lco_future_new(0);
    hpx_addr_t rsync = hpx_lco_future_new(0);
    hpx_addr_t fut = hpx_lco_newfuture_new(sizeof(uint64_t));
    hpx_call(HPX_THERE(1), t06_get_value, &fut, sizeof(future), done);
    hpx_lco_newfuture_setat(fut, 0, sizeof(SET_VALUE), SET_VALUE, lsync, rsync);
    hpx_lco_wait(lsync);
    hpx_lco_wait(rsync);
    hpx_lco_wait(done);
    
    hpx_lco_newfuture_free(fut);
    
    printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  }
} 
END_TEST

//****************************************************************************
// hpx_lco_newfuture_getat test: 
// This tests the getting of a future's value.
//****************************************************************************
int t06_getat_action(hpx_addr_t *fut) {
  SET_VALUE_T value;
  hpx_lco_newfuture_getat(*fut, 0, sizeof(value), &value);
  ck_assert_msg(value == SET_VALUE, "Future did not contain the correct value.");
  return HPX_SUCCESS;
}

START_TEST (test_hpx_lco_newfuture_getat)
{
  printf("Starting the hpx_lco_newfuture_waitat() full test\n");

  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t done = hpx_lco_future_new(0);
  hpx_addr_t lsync = hpx_lco_future_new(0);
  hpx_addr_t rsync = hpx_lco_future_new(0);
  hpx_addr_t fut = hpx_lco_newfuture_new(sizeof(uint64_t));
  hpx_call(HPX_HERE, t06_get_value, &fut, sizeof(future), done);
  hpx_lco_newfuture_setat(fut, 0, sizeof(SET_VALUE), SET_VALUE, lsync, rsync);
  hpx_lco_wait(lsync);
  hpx_lco_wait(rsync);
  hpx_lco_wait(done);
  
  hpx_lco_newfuture_free(fut);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
} 
END_TEST

//****************************************************************************
// hpx_lco_newfuture_getat remote test: 
// This tests the getting of a future's value at a remote location.
//****************************************************************************
int t06_getat_action(hpx_addr_t *fut) {
  SET_VALUE_T value;
  hpx_lco_newfuture_getat(*fut, 0, sizeof(value), &value);
  ck_assert_msg(value == SET_VALUE, "Future did not contain the correct value.");
  return HPX_SUCCESS;
}

START_TEST (test_hpx_lco_newfuture_waitat_full)
{
  if (hpx_get_num_localities() > 0) {
    printf("Starting the hpx_lco_newfuture_waitat() full test\n");
    
    // allocate and start a timer
    hpx_time_t t1 = hpx_time_now();
    
    hpx_addr_t done = hpx_lco_future_new(0);
    hpx_addr_t lsync = hpx_lco_future_new(0);
    hpx_addr_t rsync = hpx_lco_future_new(0);
    hpx_addr_t fut = hpx_lco_newfuture_new(sizeof(uint64_t));
    hpx_call(HPX_THERE(1), t06_get_value, &fut, sizeof(future), done);
    hpx_lco_newfuture_setat(fut, 0, sizeof(SET_VALUE), SET_VALUE, lsync, rsync);
    hpx_lco_wait(lsync);
    hpx_lco_wait(rsync);
    hpx_lco_wait(done);
    
    hpx_lco_newfuture_free(fut);
    
    printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  }
} 
END_TEST

//****************************************************************************
// hpx_lco_newfuture_waitat_for test: 
// This tests waiting on a future for a set amount of time.
//****************************************************************************

// HERE HERE HERE

int t06_waitat_for_action(hpx_addr_t *fut) {
  SET_VALUE_T value;
  hpx_time_t t1 = hpx_time_now();

  hpx_time_t *duration = hpx_time_construct(5, 0);
  hpx_lco_newfuture_waitat_for(*fut, 0, HPX_SET);

  hpx_time_t t2 = hpx_time_now();
  ck_assert_msg(hpx_time_diff_ms(t1, t2) > 5000.0, "hpx_lco_newfuture_waitat_for() did not wait as long as the specified duration.");

  hpx_lco_newfuture_getat(*fut, 0, sizeof(value), &value);
  ck_assert_msg(value == SET_VALUE, "Future did not contain the correct value.");

  return HPX_SUCCESS;
}

START_TEST (test_hpx_lco_newfuture_getat)
{
  printf("Starting the hpx_lco_newfuture_waitat() full test\n");

  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t done = hpx_lco_future_new(0);
  hpx_addr_t lsync = hpx_lco_future_new(0);
  hpx_addr_t rsync = hpx_lco_future_new(0);
  hpx_addr_t fut = hpx_lco_newfuture_new(sizeof(uint64_t));
  hpx_call(HPX_HERE, t06_get_value, &fut, sizeof(future), done);
  sleep(6);
  hpx_lco_newfuture_setat(fut, 0, sizeof(SET_VALUE), SET_VALUE, lsync, rsync);
  hpx_lco_wait(lsync);
  hpx_lco_wait(rsync);
  hpx_lco_wait(done);
  
  hpx_lco_newfuture_free(fut);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
} 
END_TEST



//******

// TODO
// 0. is shared() (how!?)
// 1. waitat_for
// 2. waitat_until
// 3., etc. array versions of above

//*****




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
  printf("Starting the array of futures test\n");
  uint64_t value = 0;
  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();
  
  // allocate 2 futures with size of each future's value and the  
  // one future per block
  hpx_addr_t base = hpx_lco_future_array_new(2, sizeof(uint64_t), 1);
  hpx_addr_t other = hpx_lco_future_array_at(base, 1);

  hpx_call_sync(other, t06_get_future_value, NULL, 0, &value, sizeof(value));
  printf("value = %"PRIu64"\n", value);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
}
END_TEST

//****************************************************************************
// Register tests from this file
//****************************************************************************

void add_06_TestFutures(TCase *tc) {
  tcase_add_test(tc, test_libhpx_lco_future_new);
  tcase_add_test(tc, test_libhpx_lco_future_array);
}
