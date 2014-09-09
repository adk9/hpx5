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
#include <stdio.h>
#include <stdlib.h>
#include "tests.h"
#include "hpx/hpx.h"

#define SET_VALUE_T int
static int SET_VALUE = 1234;
static int NUM_LOCAL_FUTURES = 4;

//****************************************************************************
// hpx_lco_newfuture_waitat for empty test: 
// This tests the creation of a single future. In the test, we wait on the
// future to see if it is empty, since a future should be empty on creation.
// Finally we free the future.
//****************************************************************************
int t06_waitforempty_action(hpx_addr_t *fut) {
  hpx_lco_newfuture_waitat(*fut, 0, HPX_UNSET);
  return HPX_SUCCESS;
}

START_TEST (test_hpx_lco_newfuture_waitat_empty)
{
  printf("Starting the hpx_lco_newfuture_waitat() empty test\n");

  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t done = hpx_lco_future_new(0);
  hpx_addr_t fut = hpx_lco_newfuture_new(sizeof(uint64_t));
  hpx_call(HPX_HERE, t06_waitforempty, &fut, sizeof(fut), done);
  hpx_lco_wait(done);
  
  hpx_lco_newfuture_free(fut);
  hpx_lco_delete(done, HPX_NULL);

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
  if (hpx_get_num_ranks() > 1) {

    printf("Starting the hpx_lco_newfuture_waitat() empty (remote) test\n");
    
    // allocate and start a timer
    hpx_time_t t1 = hpx_time_now();
    
    hpx_addr_t done = hpx_lco_future_new(0);
    hpx_addr_t fut = hpx_lco_newfuture_new(sizeof(uint64_t));
    hpx_call(HPX_THERE(1), t06_waitforempty, &fut, sizeof(fut), done);
    hpx_lco_wait(done);
    
    hpx_lco_newfuture_free(fut);
    hpx_lco_delete(done, HPX_NULL);

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
  hpx_lco_newfuture_waitat(*fut, 0, HPX_SET);
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
  hpx_call(HPX_HERE, t06_waitforfull, &fut, sizeof(fut), done);
  hpx_lco_newfuture_setat(fut, 0, sizeof(SET_VALUE), &SET_VALUE, lsync, rsync);
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
  if (hpx_get_num_ranks() > 1) {
    printf("Starting the hpx_lco_newfuture_waitat() full remote test\n");

    // allocate and start a timer
    hpx_time_t t1 = hpx_time_now();
    
    hpx_addr_t done = hpx_lco_future_new(0);
    hpx_addr_t lsync = hpx_lco_future_new(0);
    hpx_addr_t rsync = hpx_lco_future_new(0);
    hpx_addr_t fut = hpx_lco_newfuture_new(sizeof(uint64_t));
    hpx_call(HPX_THERE(1), t06_waitforfull, &fut, sizeof(fut), done);
    hpx_lco_newfuture_setat(fut, 0, sizeof(SET_VALUE), &SET_VALUE, lsync, rsync);
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
  printf("Starting the hpx_lco_newfuture_getat() test\n");

  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t done = hpx_lco_future_new(0);
  hpx_addr_t lsync = hpx_lco_future_new(0);
  hpx_addr_t rsync = hpx_lco_future_new(0);
  hpx_addr_t fut = hpx_lco_newfuture_new(sizeof(uint64_t));
  hpx_call(HPX_HERE, t06_getat, &fut, sizeof(fut), done);
  hpx_lco_newfuture_setat(fut, 0, sizeof(SET_VALUE), &SET_VALUE, lsync, rsync);
  hpx_lco_wait(lsync);
  hpx_lco_wait(rsync);
  hpx_lco_wait(done);
  
  hpx_lco_newfuture_free(fut);
  hpx_lco_delete(lsync, HPX_NULL);
  hpx_lco_delete(rsync, HPX_NULL);
  hpx_lco_delete(done, HPX_NULL);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
} 
END_TEST

//****************************************************************************
// hpx_lco_newfuture_getat remote test: 
// This tests the getting of a future's value at a remote location.
//****************************************************************************
START_TEST (test_hpx_lco_newfuture_getat_remote)
{
  if (hpx_get_num_ranks() > 0) {
    printf("Starting the hpx_lco_newfuture_getat() remote test\n");
    
    // allocate and start a timer
    hpx_time_t t1 = hpx_time_now();
    
    hpx_addr_t done = hpx_lco_future_new(0);
    hpx_addr_t lsync = hpx_lco_future_new(0);
    hpx_addr_t rsync = hpx_lco_future_new(0);
    hpx_addr_t fut = hpx_lco_newfuture_new(sizeof(uint64_t));
    hpx_call(HPX_THERE(1), t06_getat, &fut, sizeof(fut), done);
    hpx_lco_newfuture_setat(fut, 0, sizeof(SET_VALUE), &SET_VALUE, lsync, rsync);
    hpx_lco_wait(lsync);
    hpx_lco_wait(rsync);
    hpx_lco_wait(done);
    
    hpx_lco_newfuture_free(fut);
    hpx_lco_delete(lsync, HPX_NULL);
    hpx_lco_delete(rsync, HPX_NULL);
    hpx_lco_delete(done, HPX_NULL);
    
    printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  }
} 
END_TEST

//****************************************************************************
// This testcase tests for future wait for function.
// Waits for the result to become available. Blocks until specified timeout
// _duration has elapsed or the result becomes available, whichever comes
// first. Returns value identifies the state of the result.
// This function may block for longer than timeout_duration due to
// scheduling or resource contention delay
//****************************************************************************
START_TEST (test_hpx_lco_newfuture_waitfor) 
{
  printf("Starting the future wait for test\n");
  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t fut = hpx_lco_newfuture_new(sizeof(uint64_t));
  hpx_lco_newfuture_setat(fut, 0, sizeof(SET_VALUE), &SET_VALUE, 
                          HPX_NULL, HPX_NULL);

  printf("Waiting for status to be set...\n");
  hpx_future_status status;
  do {
    hpx_time_t timeout_duration = hpx_time_construct(5, 0);
    status = hpx_lco_newfuture_waitat_for(fut, 0, HPX_SET, timeout_duration);
    if (status == HPX_FUTURE_STATUS_DEFERRED) {
      printf("Deferred\n");
    } else if (status == HPX_FUTURE_STATUS_TIMEOUT) {
      printf("Timeout\n");
    } else if (status == HPX_FUTURE_STATUS_READY) {
      printf("Ready\n");
    }
  } while (status != HPX_FUTURE_STATUS_READY);

  uint64_t result;
  hpx_lco_newfuture_getat(fut, 0, sizeof(uint64_t), &result);
  printf("Result of the future is = %"PRIu64"\n", result);
  ck_assert(result == SET_VALUE);

  hpx_lco_newfuture_free(fut);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
}
END_TEST

//****************************************************************************
// wait_until waits for a result to become available. It blocks until 
// specified timeout_time has been reached or the result becomes available,
// whichever comes first. The return value indicates why wait_until returned.
// The behavior is undefined if valid()== false before the call 
// to this function. In this case throw a future error with an error condition
// no state.
//****************************************************************************
START_TEST (test_hpx_lco_newfuture_waituntil) 
{
  printf("Starting the future wait for test\n");
  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t fut = hpx_lco_newfuture_new(sizeof(uint64_t));
  hpx_lco_newfuture_setat(fut, 0, sizeof(SET_VALUE), &SET_VALUE,
                          HPX_NULL, HPX_NULL);

  printf("Waiting for status to be set...\n");
  hpx_future_status status;
  do {
    hpx_time_t now = hpx_time_now();
    // Duration is used to measure the time since epoch
    hpx_time_t duration = hpx_time_construct(5, 0);
    hpx_time_t timeout_time = hpx_time_point(now, duration);
    status = hpx_lco_newfuture_waitat_until(fut, 0, HPX_SET, timeout_time);
    if (status == HPX_FUTURE_STATUS_DEFERRED) {
      printf("Deferred\n");
    } else if (status == HPX_FUTURE_STATUS_TIMEOUT) {
      printf("Timeout\n");
    } else if (status == HPX_FUTURE_STATUS_READY) {
      printf("Ready\n");
    }
  } while (status != HPX_FUTURE_STATUS_READY);

  uint64_t result;
  hpx_lco_newfuture_getat(fut, 0, sizeof(uint64_t), &result);
  printf("Result of the future is = %"PRIu64"\n", result);
  ck_assert(result == SET_VALUE);

  hpx_lco_newfuture_free(fut);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
}
END_TEST

//****************************************************************************
// hpx_lco_newfuture_waitat for empty test, array version: 
// This tests the creation of an array of futures. In the test, we wait on the
// futures to see if they are empty, since a future should be empty on creation.
// Finally we free the futures.
//****************************************************************************
struct waitforempty_id_args {
  hpx_addr_t base;
  int index;
};

int t06_waitforempty_id_action(void *vargs) {
  struct waitforempty_id_args *args = (struct waitforempty_id_args*)vargs; 
  hpx_lco_newfuture_waitat(args->base, args->index, HPX_UNSET);
  return HPX_SUCCESS;
}

START_TEST (test_hpx_lco_newfuture_waitat_empty_array)
{
  printf("Starting the hpx_lco_newfuture_waitat() empty array test\n");

  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t done = hpx_lco_and_new(NUM_LOCAL_FUTURES);
  hpx_addr_t fut = hpx_lco_newfuture_new_all(NUM_LOCAL_FUTURES, sizeof(uint64_t));
  struct waitforempty_id_args *args = calloc(NUM_LOCAL_FUTURES, sizeof(args[0]));
  for (int i = 0; i < NUM_LOCAL_FUTURES; i++) {
    args[i].base = fut;
    args[i].index = i;
    hpx_call(HPX_HERE, t06_waitforempty_id, &args[i], sizeof(args[i]), done);
  }

  hpx_lco_wait(done);
  
  hpx_lco_newfuture_free_all(fut);
  hpx_lco_delete(done, HPX_NULL);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
} 
END_TEST


//****************************************************************************
// hpx_lco_newfuture_waitat for empty remote test, array version: 
// This tests the creation of an array of futures. In the test, we wait on the
// futures to see if they are empty, since a future should be empty on creation.
// Finally we free the futures.
//****************************************************************************
START_TEST (test_hpx_lco_newfuture_waitat_empty_array_remote)
{
  int ranks = hpx_get_num_ranks();
  if (ranks > 1) {
    printf("Starting the hpx_lco_newfuture_waitat() empty array remote test\n");
    
    // allocate and start a timer
    hpx_time_t t1 = hpx_time_now();
    
    hpx_addr_t done = hpx_lco_and_new(NUM_LOCAL_FUTURES * ranks);
    hpx_addr_t fut = hpx_lco_newfuture_new_all(NUM_LOCAL_FUTURES * ranks, sizeof(uint64_t));
    struct waitforempty_id_args *args = calloc(NUM_LOCAL_FUTURES * ranks, sizeof(args[0]));
    for (int i = 0; i < NUM_LOCAL_FUTURES * ranks; i++) {
      args[i].base = fut;
      args[i].index = i;
      hpx_call(hpx_lco_newfuture_at(fut, i), t06_waitforempty_id, &args[i], sizeof(args[i]), done);
    }
    
    hpx_lco_wait(done);
    hpx_lco_newfuture_free_all(fut);
    hpx_lco_delete(done, HPX_NULL);
    
    printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  }
} 
END_TEST

//****************************************************************************
// hpx_lco_newfuture_waitat for full test, array version
//****************************************************************************
struct waitforfull_id_args {
  hpx_addr_t base;
  int index;
};

int t06_waitforfull_id_action(void *vargs) {
  struct waitforfull_id_args *args = (struct waitforfull_id_args*)vargs; 
  hpx_lco_newfuture_waitat(args->base, args->index, HPX_SET);
  return HPX_SUCCESS;
}

START_TEST (test_hpx_lco_newfuture_waitat_full_array)
{
  printf("Starting the hpx_lco_newfuture_waitat() full array test\n");

  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t done = hpx_lco_and_new(NUM_LOCAL_FUTURES);
  hpx_addr_t fut = hpx_lco_newfuture_new_all(NUM_LOCAL_FUTURES, sizeof(uint64_t));
  struct waitforempty_id_args *args = calloc(NUM_LOCAL_FUTURES, sizeof(args[0]));
  for (int i = 0; i < NUM_LOCAL_FUTURES; i++) {
    args[i].base = fut;
    args[i].index = i;
    hpx_call(HPX_HERE, t06_waitforfull_id, &args[i], sizeof(args[i]), done);
  }

  for (int i = 0; i < NUM_LOCAL_FUTURES; i++) {
    hpx_addr_t lsync = hpx_lco_future_new(0);
    hpx_addr_t rsync = hpx_lco_future_new(0);
    hpx_lco_newfuture_setat(fut, 0, sizeof(SET_VALUE), &SET_VALUE, lsync, rsync);
    hpx_lco_delete(lsync, HPX_NULL);
    hpx_lco_delete(rsync, HPX_NULL);
  }
  hpx_lco_wait(done);  
  hpx_lco_newfuture_free_all(fut);
  hpx_lco_delete(done, HPX_NULL);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
} 
END_TEST

//****************************************************************************
// hpx_lco_newfuture_waitat for full remote test, array version
//****************************************************************************
START_TEST (test_hpx_lco_newfuture_waitat_full_array_remote)
{
  int ranks = hpx_get_num_ranks();
  if (ranks > 1) {
    printf("Starting the hpx_lco_newfuture_waitat() full array remote test\n");
    
    // allocate and start a timer
    hpx_time_t t1 = hpx_time_now();
    
    hpx_addr_t done = hpx_lco_and_new(NUM_LOCAL_FUTURES * ranks);
    hpx_addr_t fut = hpx_lco_newfuture_new_all(NUM_LOCAL_FUTURES * ranks, sizeof(uint64_t));
    struct waitforempty_id_args *args = calloc(NUM_LOCAL_FUTURES * ranks, sizeof(args[0]));
    for (int i = 0; i < NUM_LOCAL_FUTURES * ranks; i++) {
      args[i].base = fut;
      args[i].index = i;
      hpx_call(hpx_lco_newfuture_at(fut, i), t06_waitforfull_id, &args[i], sizeof(args[i]), done);
    }
    for (int i = 0; i < NUM_LOCAL_FUTURES * ranks; i++) {
      hpx_addr_t lsync = hpx_lco_future_new(0);
      hpx_addr_t rsync = hpx_lco_future_new(0);
      hpx_lco_newfuture_setat(fut, 0, sizeof(SET_VALUE), &SET_VALUE, lsync, rsync);
      hpx_lco_delete(lsync, HPX_NULL);
      hpx_lco_delete(rsync, HPX_NULL);
    }
   
    hpx_lco_wait(done);
    hpx_lco_newfuture_free_all(fut);
    hpx_lco_delete(done, HPX_NULL);
    
    printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  } 
}
END_TEST

//****************************************************************************
// hpx_lco_newfuture_getat test: 
// This tests the getting of a future's value.
//****************************************************************************
int t06_getat_id_action(void* vargs) {
  struct waitforfull_id_args *args = (struct waitforfull_id_args*)vargs; 
  SET_VALUE_T value;
  hpx_lco_newfuture_getat(args->base, args->index, sizeof(value), &value);
  ck_assert_msg(value == SET_VALUE, "Future did not contain the correct value.");
  return HPX_SUCCESS;
}

START_TEST (test_hpx_lco_newfuture_getat_array)
{
  printf("Starting the hpx_lco_newfuture_getat() array test\n");

  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t done = hpx_lco_and_new(NUM_LOCAL_FUTURES);
  hpx_addr_t fut = hpx_lco_newfuture_new_all(NUM_LOCAL_FUTURES, sizeof(uint64_t));
  struct waitforempty_id_args *args = calloc(NUM_LOCAL_FUTURES, sizeof(args[0]));
  for (int i = 0; i < NUM_LOCAL_FUTURES; i++) {
    args[i].base = fut;
    args[i].index = i;
    hpx_call(HPX_HERE, t06_getat_id, &args[i], sizeof(args[i]), done);
  }

  for (int i = 0; i < NUM_LOCAL_FUTURES; i++) {
    hpx_addr_t lsync = hpx_lco_future_new(0);
    hpx_addr_t rsync = hpx_lco_future_new(0);
    hpx_lco_newfuture_setat(fut, 0, sizeof(SET_VALUE), &SET_VALUE, lsync, rsync);
    hpx_lco_delete(lsync, HPX_NULL);
    hpx_lco_delete(rsync, HPX_NULL);
  }
  hpx_lco_wait(done);  
  hpx_lco_newfuture_free_all(fut);
  hpx_lco_delete(done, HPX_NULL);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
} 
END_TEST


//****************************************************************************
// hpx_lco_newfuture_getat for full remote test, array version
//****************************************************************************
START_TEST (test_hpx_lco_newfuture_getat_array_remote)
{
  int ranks = hpx_get_num_ranks();
  if (ranks > 1) {
    printf("Starting the hpx_lco_newfuture_getat() array remote test\n");
    
    // allocate and start a timer
    hpx_time_t t1 = hpx_time_now();
    
    hpx_addr_t done = hpx_lco_and_new(NUM_LOCAL_FUTURES * ranks);
    hpx_addr_t fut = hpx_lco_newfuture_new_all(NUM_LOCAL_FUTURES * ranks, sizeof(uint64_t));
    struct waitforempty_id_args *args = calloc(NUM_LOCAL_FUTURES * ranks, sizeof(args[0]));
    for (int i = 0; i < NUM_LOCAL_FUTURES * ranks; i++) {
      args[i].base = fut;
      args[i].index = i;
      hpx_call(hpx_lco_newfuture_at(fut, i), t06_getat_id, &args[i], sizeof(args[i]), done);
    }
    for (int i = 0; i < NUM_LOCAL_FUTURES * ranks; i++) {
      hpx_addr_t lsync = hpx_lco_future_new(0);
      hpx_addr_t rsync = hpx_lco_future_new(0);
      hpx_lco_newfuture_setat(fut, 0, sizeof(SET_VALUE), &SET_VALUE, lsync, rsync);
      hpx_lco_delete(lsync, HPX_NULL);
      hpx_lco_delete(rsync, HPX_NULL);
    }
   
    hpx_lco_wait(done);
    hpx_lco_newfuture_free_all(fut);
    hpx_lco_delete(done, HPX_NULL);
    
    printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  } 
}
END_TEST

//****************************************************************************
// test_hpx_lco_newfuture_wait_all
//****************************************************************************
START_TEST (test_hpx_lco_newfuture_wait_all) 
{
  printf("Starting the future wait for test\n");
  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t fut = hpx_lco_newfuture_new_all(NUM_LOCAL_FUTURES, sizeof(uint64_t));
  for (int i = 0; i < NUM_LOCAL_FUTURES; i++) {
    hpx_lco_newfuture_setat(fut, 1, sizeof(SET_VALUE), &SET_VALUE, 
			    HPX_NULL, HPX_NULL);
  }
  hpx_lco_newfuture_wait_all(NUM_LOCAL_FUTURES, fut, HPX_SET);
  hpx_lco_newfuture_free_all(fut);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
}
END_TEST

//****************************************************************************
// test_hpx_lco_newfuture_wait_all_for
//****************************************************************************
START_TEST (test_hpx_lco_newfuture_wait_all_for) 
{
  printf("Starting the future wait for test\n");
  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t fut = hpx_lco_newfuture_new_all(NUM_LOCAL_FUTURES, sizeof(uint64_t));
  hpx_time_t timeout_duration = hpx_time_construct(0, 5e8);
  hpx_future_status status;
  status = hpx_lco_newfuture_wait_all_for(NUM_LOCAL_FUTURES, fut, HPX_SET, timeout_duration);
  ck_assert_msg(status = HPX_FUTURE_STATUS_TIMEOUT);
  for (int i = 0; i < NUM_LOCAL_FUTURES; i++) {
    hpx_lco_newfuture_setat(fut, 1, sizeof(SET_VALUE), &SET_VALUE, 
			    HPX_NULL, HPX_NULL);
  }
  status = hpx_lco_newfuture_wait_all_for(NUM_LOCAL_FUTURES, fut, HPX_SET, timeout_duration);
  ck_assert_msg(status = HPX_FUTURE_STATUS_READY);

  hpx_lco_newfuture_free_all(fut);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
}
END_TEST

//****************************************************************************
// test_hpx_lco_newfuture_wait_all_until
//****************************************************************************
START_TEST (test_hpx_lco_newfuture_wait_all_until) 
{
  hpx_time_t now, duration, timeout;
  printf("Starting the future wait for test\n");
  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t fut = hpx_lco_newfuture_new_all(NUM_LOCAL_FUTURES, sizeof(uint64_t));

  now = hpx_time_now();
  duration = hpx_time_construct(0, 5e8);
  timeout = hpx_time_point(now, duration);
  hpx_future_status status;
  status = hpx_lco_newfuture_wait_all_until(NUM_LOCAL_FUTURES, fut, HPX_SET, timeout);
  ck_assert_msg(status = HPX_FUTURE_STATUS_TIMEOUT);
  for (int i = 0; i < NUM_LOCAL_FUTURES; i++) {
    hpx_lco_newfuture_setat(fut, 1, sizeof(SET_VALUE), &SET_VALUE, 
			    HPX_NULL, HPX_NULL);
  }
  now = hpx_time_now();
  duration = hpx_time_construct(0, 5e8);
  timeout = hpx_time_point(now, duration);
  status = hpx_lco_newfuture_wait_all_until(NUM_LOCAL_FUTURES, fut, HPX_SET, timeout);
  ck_assert_msg(status = HPX_FUTURE_STATUS_READY);

  hpx_lco_newfuture_free_all(fut);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
}
END_TEST


//******

// TODO
// 0. is shared() (how!?)

//*****


//****************************************************************************
// Register tests from this file
//****************************************************************************

void add_06_TestNewFutures(TCase *tc) {
  tcase_add_test(tc, test_hpx_lco_newfuture_waitat_empty);
  tcase_add_test(tc, test_libhpx_lco_newfuture_waitat_empty_remote);
  tcase_add_test(tc, test_hpx_lco_newfuture_waitat_full);
  tcase_add_test(tc, test_hpx_lco_newfuture_waitat_full_remote);
  tcase_add_test(tc, test_hpx_lco_newfuture_getat);
  tcase_add_test(tc, test_hpx_lco_newfuture_getat_remote);
  tcase_add_test(tc, test_hpx_lco_newfuture_getat);
  tcase_add_test(tc, test_hpx_lco_newfuture_waitfor);
  tcase_add_test(tc, test_hpx_lco_newfuture_waituntil);
  tcase_add_test(tc, test_hpx_lco_newfuture_waitat_empty_array);
  tcase_add_test(tc, test_hpx_lco_newfuture_waitat_empty_array_remote);
  tcase_add_test(tc, test_hpx_lco_newfuture_waitat_full_array);
  tcase_add_test(tc, test_hpx_lco_newfuture_waitat_full_array_remote);
  tcase_add_test(tc, test_hpx_lco_newfuture_getat_array);
  tcase_add_test(tc, test_hpx_lco_newfuture_getat_array_remote);
  tcase_add_test(tc, test_hpx_lco_newfuture_wait_all);
  //  tcase_add_test(tc, test_hpx_lco_newfuture_wait_all_remote);
  //  tcase_add_test(tc, test_hpx_lco_newfuture_get_all);
  //  tcase_add_test(tc, test_hpx_lco_newfuture_get_all_remote);
  tcase_add_test(tc, test_hpx_lco_newfuture_wait_all_for);
  tcase_add_test(tc, test_hpx_lco_newfuture_wait_all_until);
}
