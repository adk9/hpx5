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

// Goal of this testcase is to test future LCO

#include <stdio.h>
#include <stdlib.h>
#include "tests.h"
#include "hpx/hpx.h"

#define SET_VALUE_T int
static int SET_VALUE = 1234;
static int NUM_LOCAL_FUTURES = 4;

// hpx_lco_newfuture_waitat for empty test: 
// This tests the creation of a single future. In the test, we wait on the
// future to see if it is empty, since a future should be empty on creation.
// Finally we free the future.
static HPX_ACTION(_waitforempty, hpx_netfuture_t fut) {
  hpx_lco_newfuture_waitat(fut, 0, HPX_UNSET);
  return HPX_SUCCESS;
}

static HPX_ACTION(lco_newfuture_waitat_empty, void *UNUSED) {
  printf("Starting the hpx_lco_newfuture_waitat() empty test\n");

  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t done = hpx_lco_future_new(0);
  hpx_netfuture_t fut = hpx_lco_newfuture_new(sizeof(SET_VALUE_T));
  hpx_call(HPX_HERE, _waitforempty, done, fut, sizeof(fut));
  hpx_lco_wait(done);
  
  hpx_lco_newfuture_free(fut);
  hpx_lco_delete(done, HPX_NULL);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
} 

// hpx_lco_newfuture_waitat for empty (remote version) test:
// Just like the hpx_lco_newfuture_waitat test but this time we wait
// remotely (if we have more than one locality)
static HPX_ACTION(test_libhpx_lco_newfuture_waitat_empty_remote, void *UNUSED) {
  if (hpx_get_num_ranks() > 1) {

    printf("Starting the hpx_lco_newfuture_waitat() empty (remote) test\n");
    
    // allocate and start a timer
    hpx_time_t t1 = hpx_time_now();
    
    hpx_addr_t done = hpx_lco_future_new(0);
    hpx_netfuture_t fut = hpx_lco_newfuture_new(sizeof(SET_VALUE_T));
    hpx_call(HPX_THERE(1), _waitforempty, done, fut, sizeof(fut));
    hpx_lco_wait(done);
    
    hpx_lco_newfuture_free(fut);
    hpx_lco_delete(done, HPX_NULL);

    printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  }
} 

// hpx_lco_newfuture_waitat full test: 
// This tests the waiting for a future to be set. This test includes setting
// the future, but we do not actually test the value that gets set.
static HPX_ACTION(_waitforfull, hpx_netfuture_t fut) {
  hpx_lco_newfuture_waitat(fut, 0, HPX_SET);
  return HPX_SUCCESS;
}

static HPX_ACTION(lco_newfuture_waitat_full, void *UNUSED) {
  printf("Starting the hpx_lco_newfuture_waitat() full test\n");

  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t done = hpx_lco_future_new(0);
  hpx_addr_t lsync = hpx_lco_future_new(0);
  hpx_addr_t rsync = hpx_lco_future_new(0);
  hpx_netfuture_t fut = hpx_lco_newfuture_new(sizeof(SET_VALUE_T));
  hpx_call(HPX_HERE, _waitforfull, done, fut, sizeof(fut));
  hpx_lco_newfuture_setat(fut, 0, sizeof(SET_VALUE), &SET_VALUE, lsync, rsync);
  hpx_addr_t syncs[] = {lsync, rsync};
  hpx_lco_wait_all(2, syncs, NULL);
  hpx_lco_wait(done);

  hpx_lco_delete(lsync, HPX_NULL);
  hpx_lco_delete(rsync, HPX_NULL);
  hpx_lco_newfuture_free(fut);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
} 

// hpx_lco_newfuture_waitat full remote test: 
// This tests the setting of a single future and waiting for a future to be set.
static HPX_ACTION(lco_newfuture_waitat_full_remote, void *UNUSED) {
  if (hpx_get_num_ranks() > 1) {
    printf("Starting the hpx_lco_newfuture_waitat() full remote test\n");

    // allocate and start a timer
    hpx_time_t t1 = hpx_time_now();
    
    hpx_addr_t done = hpx_lco_future_new(0);
    hpx_addr_t lsync = hpx_lco_future_new(0);
    hpx_addr_t rsync = hpx_lco_future_new(0);
    hpx_netfuture_t fut = hpx_lco_newfuture_new(sizeof(SET_VALUE_T));
    hpx_call(HPX_THERE(1), _waitforfull, done, fut, sizeof(fut));
    hpx_lco_newfuture_setat(fut, 0, sizeof(SET_VALUE), &SET_VALUE, lsync, rsync);
    hpx_addr_t syncs[] = {lsync, rsync};
    hpx_lco_wait_all(2, syncs, NULL);

    hpx_lco_wait(done);
    
    hpx_lco_newfuture_free(fut);
    hpx_lco_delete(lsync, HPX_NULL);
    hpx_lco_delete(rsync, HPX_NULL);

    printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  }
} 

// hpx_lco_newfuture_getat test: 
// This tests the getting of a future's value.
static HPX_ACTION(_getat, hpx_netfuture_t fut) {
  SET_VALUE_T value;
  hpx_lco_newfuture_getat(fut, 0, sizeof(value), &value);
  hpx_thread_continue(sizeof(value), &value);
}

static HPX_ACTION(lco_newfuture_getat, void *UNUSED) {
  printf("Starting the hpx_lco_newfuture_getat() test\n");

  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t done = hpx_lco_future_new(sizeof(SET_VALUE));
  hpx_addr_t lsync = hpx_lco_future_new(0);
  hpx_addr_t rsync = hpx_lco_future_new(0);
  hpx_netfuture_t fut = hpx_lco_newfuture_new(sizeof(SET_VALUE_T));
  hpx_call(HPX_HERE, _getat, done, fut, sizeof(fut));
  hpx_lco_newfuture_setat(fut, 0, sizeof(SET_VALUE), &SET_VALUE, lsync, rsync);
  hpx_lco_wait(lsync);
  hpx_lco_wait(rsync);
  SET_VALUE_T value;
  hpx_lco_get(done, sizeof(SET_VALUE), &value);
  assert_msg(value == SET_VALUE, "Future did not contain the correct value.");
  
  hpx_lco_newfuture_free(fut);
  hpx_lco_delete(lsync, HPX_NULL);
  hpx_lco_delete(rsync, HPX_NULL);
  hpx_lco_delete(done, HPX_NULL);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  return HPX_SUCCESS;
} 

// hpx_lco_newfuture_getat remote test: 
// This tests the getting of a future's value at a remote location.
static HPX_ACTION(lco_newfuture_getat_remote, void *UNUSED) {
  if (hpx_get_num_ranks() > 0) {
    printf("Starting the hpx_lco_newfuture_getat() remote test\n");
    
    // allocate and start a timer
    hpx_time_t t1 = hpx_time_now();
    
    hpx_addr_t done = hpx_lco_future_new(sizeof(SET_VALUE));
    hpx_addr_t lsync = hpx_lco_future_new(0);
    hpx_addr_t rsync = hpx_lco_future_new(0);
    hpx_netfuture_t fut = hpx_lco_newfuture_new(sizeof(SET_VALUE_T));
    hpx_call(HPX_THERE(1), _getat, done, fut, sizeof(fut));
    hpx_lco_newfuture_setat(fut, 0, sizeof(SET_VALUE), &SET_VALUE, lsync, rsync);
    hpx_addr_t syncs[] = {lsync, rsync};
    hpx_lco_wait_all(2, syncs, NULL);
    SET_VALUE_T value;
    hpx_lco_get(done, sizeof(SET_VALUE), &value);
    assert_msg(value == SET_VALUE, "Future did not contain the correct value.");

    hpx_lco_newfuture_free(fut);
    hpx_lco_delete(lsync, HPX_NULL);
    hpx_lco_delete(rsync, HPX_NULL);
    hpx_lco_delete(done, HPX_NULL);
    
    printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  }
  return HPX_SUCCESS;
} 

// This testcase tests for future wait for function.
// Waits for the result to become available. Blocks until specified timeout
// _duration has elapsed or the result becomes available, whichever comes
// first. Returns value identifies the state of the result.
// This function may block for longer than timeout_duration due to
// scheduling or resource contention delay
/*
static HPX_ACTION(lco_newfuture_waitfor) 
{
  printf("Starting the future wait for test\n");
  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t fut = hpx_lco_newfuture_new(sizeof(SET_VALUE_T));
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

  SET_VALUE_T result;
  hpx_lco_newfuture_getat(fut, 0, sizeof(SET_VALUE_T), &result);
  printf("Result of the future is = %d\n", result);
  assert(result == SET_VALUE);

  hpx_lco_newfuture_free(fut);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
}
*/
// wait_until waits for a result to become available. It blocks until 
// specified timeout_time has been reached or the result becomes available,
// whichever comes first. The return value indicates why wait_until returned.
// The behavior is undefined if valid()== false before the call 
// to this function. In this case throw a future error with an error condition
// no state.
/*
static HPX_ACTION(lco_newfuture_waituntil) 
{
  printf("Starting the future wait for test\n");
  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t fut = hpx_lco_newfuture_new(sizeof(SET_VALUE_T));
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

  SET_VALUE_T result;
  hpx_lco_newfuture_getat(fut, 0, sizeof(SET_VALUE_T), &result);
  printf("Result of the future is = %d\n", result);
  assert(result == SET_VALUE);

  hpx_lco_newfuture_free(fut);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
}
*/
// Testcase to test shared future
static HPX_ACTION(_lcoSetGet, int *args) {
  hpx_thread_continue(sizeof(SET_VALUE_T), &SET_VALUE);
}

#if 0
static HPX_ACTION(lco_newfuture_shared, void *UNUSED) {
    int n = 10, result;
    printf("Starting the hpx_lco_newfuture_shared_new test\n");
    // allocate and start a timer
    hpx_time_t t1 = hpx_time_now();

    hpx_netfuture_t shared_done = hpx_lco_newfuture_shared_new(sizeof(SET_VALUE_T));
    hpx_call(HPX_HERE, _lcoSetGet, shared_done, &n, sizeof(n));
    
    // Shared futures can be accessed multiple times;
    hpx_lco_get(shared_done, sizeof(int), &result);
    printf("Value = %d\n", result);

    hpx_lco_get(shared_done, sizeof(int), &result);
    printf("Its double = %d\n", result * 2); 

    printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
}
#endif

// hpx_lco_newfuture_waitat for empty test, array version: 
// This tests the creation of an array of futures. In the test, we wait on the
// futures to see if they are empty, since a future should be empty on creation.
// Finally we free the futures.
struct waitforempty_id_args {
  hpx_netfuture_t base;
  int index;
};

static HPX_ACTION(_waitforempty_id, void *vargs) {
  struct waitforempty_id_args *args = (struct waitforempty_id_args*)vargs; 
  hpx_lco_newfuture_waitat(args->base, args->index, HPX_UNSET);
  return HPX_SUCCESS;
}

static HPX_ACTION(lco_newfuture_waitat_empty_array, void *UNUSED) {
  printf("Starting the hpx_lco_newfuture_waitat() empty array test\n");

  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t done = hpx_lco_and_new(NUM_LOCAL_FUTURES);
  hpx_netfuture_t fut = hpx_lco_newfuture_new_all(NUM_LOCAL_FUTURES, sizeof(SET_VALUE_T));
  struct waitforempty_id_args *args = calloc(NUM_LOCAL_FUTURES, sizeof(args[0]));
  for (int i = 0; i < NUM_LOCAL_FUTURES; i++) {
    args[i].base = fut;
    args[i].index = i;
    hpx_call(HPX_THERE(hpx_lco_newfuture_get_rank(fut)), _waitforempty_id, done, &args[i], sizeof(args[i]));
  }

  hpx_lco_wait(done);
  
  hpx_lco_newfuture_free_all(NUM_LOCAL_FUTURES, fut);
  hpx_lco_delete(done, HPX_NULL);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
} 


// hpx_lco_newfuture_waitat for empty remote test, array version: 
// This tests the creation of an array of futures. In the test, we wait on the
// futures to see if they are empty, since a future should be empty on creation.
// Finally we free the futures.
static HPX_ACTION(lco_newfuture_waitat_empty_array_remote, void *UNUSED) {
  int ranks = hpx_get_num_ranks();
  if (ranks > 1) {
    printf("Starting the hpx_lco_newfuture_waitat() empty array remote test\n");
    
    // allocate and start a timer
    hpx_time_t t1 = hpx_time_now();
    
    hpx_addr_t done = hpx_lco_and_new(NUM_LOCAL_FUTURES * ranks);
    hpx_netfuture_t fut = hpx_lco_newfuture_new_all(NUM_LOCAL_FUTURES * ranks, sizeof(SET_VALUE_T));
    struct waitforempty_id_args *args = calloc(NUM_LOCAL_FUTURES * ranks, sizeof(args[0]));
    for (int i = 0; i < NUM_LOCAL_FUTURES * ranks; i++) {
      args[i].base = fut;
      args[i].index = i;
      hpx_call(HPX_THERE(hpx_lco_newfuture_get_rank(fut)),
               _waitforempty_id, done, &args[i], sizeof(args[i]));
    }
    
    hpx_lco_wait(done);
    hpx_lco_newfuture_free_all(NUM_LOCAL_FUTURES * ranks, fut);
    hpx_lco_delete(done, HPX_NULL);
    
    printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  }
} 

// hpx_lco_newfuture_waitat for full test, array version
struct waitforfull_id_args {
  hpx_netfuture_t base;
  int index;
};

static HPX_ACTION(_waitforfull_id, void *vargs) {
  struct waitforfull_id_args *args = (struct waitforfull_id_args*)vargs; 
  hpx_lco_newfuture_waitat(args->base, args->index, HPX_SET);
  return HPX_SUCCESS;
}

static HPX_ACTION(lco_newfuture_waitat_full_array, void *UNUSED) {
  printf("Starting the hpx_lco_newfuture_waitat() full array test\n");

  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t done = hpx_lco_and_new(NUM_LOCAL_FUTURES);
  hpx_netfuture_t fut = hpx_lco_newfuture_new_all(NUM_LOCAL_FUTURES, sizeof(SET_VALUE_T));
  struct waitforempty_id_args *args = calloc(NUM_LOCAL_FUTURES, sizeof(args[0]));
  for (int i = 0; i < NUM_LOCAL_FUTURES; i++) {
    args[i].base = fut;
    args[i].index = i;
    hpx_call(HPX_HERE, _waitforfull_id, done, &args[i], sizeof(args[i]));
  }

  for (int i = 0; i < NUM_LOCAL_FUTURES; i++) {
    hpx_addr_t lsync = hpx_lco_future_new(0);
    hpx_addr_t rsync = hpx_lco_future_new(0);
    hpx_lco_newfuture_setat(fut, i, sizeof(SET_VALUE), &SET_VALUE, lsync, rsync);
    hpx_addr_t syncs[] = {lsync, rsync};
    hpx_lco_wait_all(2, syncs, NULL);
    hpx_lco_delete(lsync, HPX_NULL);
    hpx_lco_delete(rsync, HPX_NULL);
  }
  hpx_lco_wait(done);  
  hpx_lco_newfuture_free_all(NUM_LOCAL_FUTURES, fut);
  hpx_lco_delete(done, HPX_NULL);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
} 

// hpx_lco_newfuture_waitat for full remote test, array version
static HPX_ACTION(lco_newfuture_waitat_full_array_remote, void *UNUSED) {
  int ranks = hpx_get_num_ranks();
  if (ranks > 1) {
    printf("Starting the hpx_lco_newfuture_waitat() full array remote test\n");
    
    // allocate and start a timer
    hpx_time_t t1 = hpx_time_now();
    
    hpx_addr_t done = hpx_lco_and_new(NUM_LOCAL_FUTURES * ranks);
    hpx_netfuture_t fut = hpx_lco_newfuture_new_all(NUM_LOCAL_FUTURES * ranks, sizeof(SET_VALUE_T));
    struct waitforempty_id_args *args = calloc(NUM_LOCAL_FUTURES * ranks, sizeof(args[0]));
    for (int i = 0; i < NUM_LOCAL_FUTURES * ranks; i++) {
      args[i].base = fut;
      args[i].index = i;
      hpx_call(HPX_THERE(hpx_lco_newfuture_get_rank(fut)),
               _waitforfull_id, done, &args[i], sizeof(args[i]));
    }
    for (int i = 0; i < NUM_LOCAL_FUTURES * ranks; i++) {
      hpx_addr_t lsync = hpx_lco_future_new(0);
      hpx_addr_t rsync = hpx_lco_future_new(0);
      hpx_lco_newfuture_setat(fut, i, sizeof(SET_VALUE), &SET_VALUE, lsync, rsync);
      hpx_addr_t syncs[] = {lsync, rsync};
      hpx_lco_wait_all(2, syncs, NULL);
      hpx_lco_delete(lsync, HPX_NULL);
      hpx_lco_delete(rsync, HPX_NULL);
    }
   
    hpx_lco_wait(done);
    hpx_lco_newfuture_free_all(NUM_LOCAL_FUTURES * ranks, fut);
    hpx_lco_delete(done, HPX_NULL);
    
    printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  }
  return HPX_SUCCESS;
}

// hpx_lco_newfuture_getat test: 
// This tests the getting of a future's value.
static HPX_ACTION(_getat_id, void* vargs) {
  struct waitforfull_id_args *args = (struct waitforfull_id_args*)vargs; 
  SET_VALUE_T value;
  hpx_lco_newfuture_getat(args->base, args->index, sizeof(value), &value);
  // printf("Got value == %d\n", value);
  hpx_thread_continue(sizeof(value), &value);
  return HPX_SUCCESS;
}

static HPX_ACTION(lco_newfuture_getat_array, void *UNUSED) {
  printf("Starting the hpx_lco_newfuture_getat() array test\n");

  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();
  hpx_addr_t *done = malloc(sizeof(hpx_addr_t) * NUM_LOCAL_FUTURES);
  for (int i = 0; i < NUM_LOCAL_FUTURES; i++)
    done[i] = hpx_lco_future_new(sizeof(SET_VALUE));
  hpx_netfuture_t fut = hpx_lco_newfuture_new_all(NUM_LOCAL_FUTURES, sizeof(SET_VALUE_T));
  struct waitforempty_id_args *args = calloc(NUM_LOCAL_FUTURES, sizeof(args[0]));
  for (int i = 0; i < NUM_LOCAL_FUTURES; i++) {
    args[i].base = fut;
    args[i].index = i;
    hpx_call(HPX_HERE, _getat_id, done[i], &args[i], sizeof(args[i]));
  }

  for (int i = 0; i < NUM_LOCAL_FUTURES; i++) {
    hpx_addr_t lsync = hpx_lco_future_new(0);
    hpx_addr_t rsync = hpx_lco_future_new(0);
    hpx_lco_newfuture_setat(fut, i, sizeof(SET_VALUE), &SET_VALUE, lsync, rsync);
    hpx_addr_t syncs[] = {lsync, rsync};
    hpx_lco_wait_all(2, syncs, NULL);
    hpx_lco_delete(lsync, HPX_NULL);
    hpx_lco_delete(rsync, HPX_NULL);
  }
  for (int i = 0; i < NUM_LOCAL_FUTURES; i++) {
    SET_VALUE_T value;
    hpx_lco_get(done[i], sizeof(SET_VALUE), &value);
    assert_msg(value == SET_VALUE, "Future did not contain the correct value.");
  }
  hpx_lco_newfuture_free_all(NUM_LOCAL_FUTURES, fut);
  for (int i = 0; i < NUM_LOCAL_FUTURES; i++)
    hpx_lco_delete(done[i], HPX_NULL);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  return HPX_SUCCESS;
} 


// hpx_lco_newfuture_getat for full remote test, array version
static HPX_ACTION(lco_newfuture_getat_array_remote, void *UNUSED) {
  int ranks = hpx_get_num_ranks();
  if (ranks > 1) {
    printf("Starting the hpx_lco_newfuture_getat() array remote test\n");
    
    // allocate and start a timer
    hpx_time_t t1 = hpx_time_now();
    
    hpx_addr_t done = hpx_lco_and_new(NUM_LOCAL_FUTURES * ranks);
    hpx_netfuture_t fut = hpx_lco_newfuture_new_all(NUM_LOCAL_FUTURES * ranks, sizeof(SET_VALUE_T));
    struct waitforempty_id_args *args = calloc(NUM_LOCAL_FUTURES * ranks, sizeof(args[0]));
    for (int i = 0; i < NUM_LOCAL_FUTURES * ranks; i++) {
      args[i].base = fut;
      args[i].index = i;
      hpx_call(HPX_THERE(hpx_lco_newfuture_get_rank(fut)), _getat_id,
               done, &args[i], sizeof(args[i]));
    }
    for (int i = 0; i < NUM_LOCAL_FUTURES * ranks; i++) {
      hpx_addr_t lsync = hpx_lco_future_new(0);
      hpx_addr_t rsync = hpx_lco_future_new(0);
      hpx_lco_newfuture_setat(fut, i, sizeof(SET_VALUE), &SET_VALUE, lsync, rsync);
      hpx_addr_t syncs[] = {lsync, rsync};
      hpx_lco_wait_all(2, syncs, NULL);
      hpx_lco_delete(lsync, HPX_NULL);
      hpx_lco_delete(rsync, HPX_NULL);
    }
   
    hpx_lco_wait(done);
    hpx_lco_newfuture_free_all(NUM_LOCAL_FUTURES * ranks, fut);
    hpx_lco_delete(done, HPX_NULL);
    
    printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  } 
}

// lco_newfuture_wait_all
static HPX_ACTION(lco_newfuture_wait_all, void *UNUSED) {
  printf("Starting the hpx_lco_newfuture_wait_all() test\n");
  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

  hpx_netfuture_t fut = hpx_lco_newfuture_new_all(NUM_LOCAL_FUTURES, sizeof(SET_VALUE_T));
  for (int i = 0; i < NUM_LOCAL_FUTURES; i++) {
    hpx_lco_newfuture_setat(fut, i, sizeof(SET_VALUE), &SET_VALUE, 
			    HPX_NULL, HPX_NULL);
  }
  hpx_lco_newfuture_wait_all(NUM_LOCAL_FUTURES, fut, HPX_SET);
  hpx_lco_newfuture_free_all(NUM_LOCAL_FUTURES, fut);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
}

// lco_newfuture_wait_all_remote
struct _set_args {
  hpx_netfuture_t fut;
  int i;
};

static HPX_ACTION(_set, void *vargs) {
  struct _set_args *args = (struct _set_args*)vargs;
  hpx_lco_newfuture_setat(args->fut, args->i, sizeof(SET_VALUE), &SET_VALUE, HPX_NULL, HPX_NULL);
  return HPX_SUCCESS;
}

static HPX_ACTION(lco_newfuture_wait_all_remote, void *UNUSED) {
  int ranks = hpx_get_num_ranks();
  if (ranks > 1) {
    printf("Starting the hpx_lco_newfuture_wait_all() remote test\n");
    
    // allocate and start a timer
    hpx_time_t t1 = hpx_time_now();

    hpx_netfuture_t fut = hpx_lco_newfuture_new_all(NUM_LOCAL_FUTURES * ranks, sizeof(SET_VALUE_T));
    struct _set_args *args = calloc(NUM_LOCAL_FUTURES * ranks, sizeof(args[0]));
    for (int i = 0; i < NUM_LOCAL_FUTURES * ranks; i++) {
      args[i].fut = fut;
      args[i].i = i;
      hpx_call(HPX_THERE(hpx_lco_newfuture_get_rank(fut)), _set, HPX_NULL, &args[i], sizeof(args[i]));
    }
    
    hpx_lco_newfuture_wait_all(NUM_LOCAL_FUTURES * ranks, fut, HPX_SET);
    hpx_lco_newfuture_free_all(NUM_LOCAL_FUTURES * ranks, fut);
    
    printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  }
  return HPX_SUCCESS;
}

// lco_newfuture_get_all
static HPX_ACTION(lco_newfuture_get_all, void *UNUSED) {
  printf("Starting the hpx_lco_newfuture_get_all() test\n");
  
  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();
  
  hpx_netfuture_t fut = hpx_lco_newfuture_new_all(NUM_LOCAL_FUTURES, sizeof(SET_VALUE_T));
  struct _set_args *args = calloc(NUM_LOCAL_FUTURES, sizeof(args[0]));
  for (int i = 0; i < NUM_LOCAL_FUTURES; i++) {
    args[i].fut = fut;
    args[i].i = i;
    hpx_call(HPX_HERE, _set, HPX_NULL, &args[i], sizeof(args[i]));
  }
  
  SET_VALUE_T **values = calloc(NUM_LOCAL_FUTURES, sizeof(void*));
  for (int i = 0; i < NUM_LOCAL_FUTURES; i++)
    values[i] = malloc(NUM_LOCAL_FUTURES * sizeof(SET_VALUE_T));
  hpx_lco_newfuture_get_all(NUM_LOCAL_FUTURES, fut, sizeof(SET_VALUE_T), (void**)values);
  for (int i = 0; i < NUM_LOCAL_FUTURES; i++) {
    printf("FUTURE CONTAINED VALUE %d\n", *values[i]);
    assert_msg(*(int*)values[i] == SET_VALUE, "Got wrong value");
  }
  
  hpx_lco_newfuture_free_all(NUM_LOCAL_FUTURES, fut);
  for (int i = 0; i < NUM_LOCAL_FUTURES; i++)
    free(values[i]);
  free(values);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  return HPX_SUCCESS;
}

// lco_newfuture_get_all_remote
static HPX_ACTION(lco_newfuture_get_all_remote, void *UNUSED) {
  int ranks = hpx_get_num_ranks();
  if (ranks > 1) {
    printf("Starting the hpx_lco_newfuture_get_all() remote test\n");
    
    // allocate and start a timer
    hpx_time_t t1 = hpx_time_now();

    hpx_netfuture_t fut = hpx_lco_newfuture_new_all(NUM_LOCAL_FUTURES * ranks, sizeof(SET_VALUE_T));
    struct _set_args *args = calloc(NUM_LOCAL_FUTURES * ranks, sizeof(args[0]));
    for (int i = 0; i < NUM_LOCAL_FUTURES * ranks; i++) {
      args[i].fut = fut;
      args[i].i = i;
      hpx_call(HPX_THERE(hpx_lco_newfuture_get_rank(fut)), _set, HPX_NULL,
               &args[i], sizeof(args[i]));
    }
    
    SET_VALUE_T **values = calloc(NUM_LOCAL_FUTURES * ranks, sizeof(void*));
    for (int i = 0; i < NUM_LOCAL_FUTURES * ranks; i++)
      values[i] = malloc(NUM_LOCAL_FUTURES * ranks * sizeof(SET_VALUE_T));
    hpx_lco_newfuture_get_all(NUM_LOCAL_FUTURES * ranks, fut, sizeof(SET_VALUE_T), (void**)values);

    for (int i = 0; i < NUM_LOCAL_FUTURES * ranks; i++) {
      printf("FUTURE CONTAINED VALUE %d\n", *values[i]);
      assert_msg(*(int*)values[i] == SET_VALUE, "Got wrong value");
    }

    hpx_lco_newfuture_free_all(NUM_LOCAL_FUTURES * ranks, fut);
    for (int i = 0; i < NUM_LOCAL_FUTURES * ranks; i++)
      free(values[i]);
    free(values);
    
    printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  }
  return HPX_SUCCESS;
}

// lco_newfuture_wait_all_for
/*
static HPX_ACTION(lco_newfuture_wait_all_for) 
{
  printf("Starting the future wait for test\n");
  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t fut = hpx_lco_newfuture_new_all(NUM_LOCAL_FUTURES, sizeof(SET_VALUE_T));
  hpx_time_t timeout_duration = hpx_time_construct(0, 5e8);
  hpx_future_status status;
  status = hpx_lco_newfuture_wait_all_for(NUM_LOCAL_FUTURES, fut, HPX_SET, timeout_duration);
  assert(status = HPX_FUTURE_STATUS_TIMEOUT);
  for (int i = 0; i < NUM_LOCAL_FUTURES; i++) {
    hpx_lco_newfuture_setat(fut, 1, sizeof(SET_VALUE), &SET_VALUE, 
			    HPX_NULL, HPX_NULL);
  }
  status = hpx_lco_newfuture_wait_all_for(NUM_LOCAL_FUTURES, fut, HPX_SET, timeout_duration);
  assert(status = HPX_FUTURE_STATUS_READY);

  hpx_lco_newfuture_free_all(NUM_LOCAL_FUTURES, fut);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
}
*/
// lco_newfuture_wait_all_until
/*
static HPX_ACTION(lco_newfuture_wait_all_until) 
{
  hpx_time_t now, duration, timeout;
  printf("Starting the future wait for test\n");
  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t fut = hpx_lco_newfuture_new_all(NUM_LOCAL_FUTURES, sizeof(SET_VALUE_T));

  now = hpx_time_now();
  duration = hpx_time_construct(0, 5e8);
  timeout = hpx_time_point(now, duration);
  hpx_future_status status;
  status = hpx_lco_newfuture_wait_all_until(NUM_LOCAL_FUTURES, fut, HPX_SET, timeout);
  assert(status = HPX_FUTURE_STATUS_TIMEOUT);
  for (int i = 0; i < NUM_LOCAL_FUTURES; i++) {
    hpx_lco_newfuture_setat(fut, 1, sizeof(SET_VALUE), &SET_VALUE, 
			    HPX_NULL, HPX_NULL);
  }
  now = hpx_time_now();
  duration = hpx_time_construct(0, 5e8);
  timeout = hpx_time_point(now, duration);
  status = hpx_lco_newfuture_wait_all_until(NUM_LOCAL_FUTURES, fut, HPX_SET, timeout);
  assert(status = HPX_FUTURE_STATUS_READY);

  hpx_lco_newfuture_free_all(NUM_LOCAL_FUTURES, fut);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
}
*/
// Testcase to test shared future, array version
static HPX_ACTION(lco_newfuture_shared_array, void *UNUSED) {
    printf("Starting the hpx_lco_newfuture_shared_new test\n");
    // allocate and start a timer
    hpx_time_t t1 = hpx_time_now();

    hpx_netfuture_t shared_done = hpx_lco_newfuture_shared_new_all(NUM_LOCAL_FUTURES, sizeof(SET_VALUE_T));

    for (int i = 0; i < NUM_LOCAL_FUTURES; i++)
      hpx_lco_newfuture_setat(shared_done, i, sizeof(SET_VALUE), &SET_VALUE, HPX_NULL, HPX_NULL);
    
    // Shared futures can be accessed multiple times;
    for (int i = 0; i < NUM_LOCAL_FUTURES; i++) {
      SET_VALUE_T result;
      hpx_lco_newfuture_getat(shared_done, i, sizeof(SET_VALUE_T), &result);
      assert_msg(result == SET_VALUE, "Shared future did not contain the correct value on first read.");
      hpx_lco_newfuture_getat(shared_done, i, sizeof(SET_VALUE_T), &result);
      assert_msg(result == SET_VALUE, "Shared future did not contain the correct value on second read.");
    }

    printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
    return HPX_SUCCESS;
}

TEST_MAIN({
  ADD_TEST(lco_newfuture_waitat_empty);
  ADD_TEST(test_libhpx_lco_newfuture_waitat_empty_remote);
  ADD_TEST(lco_newfuture_waitat_full);
  ADD_TEST(lco_newfuture_waitat_full_remote);
  ADD_TEST(lco_newfuture_getat);
  ADD_TEST(lco_newfuture_getat_remote);
  ADD_TEST(lco_newfuture_shared);
  ADD_TEST(lco_newfuture_waitat_empty_array);
  ADD_TEST(lco_newfuture_waitat_empty_array_remote);
  ADD_TEST(lco_newfuture_waitat_full_array);
  ADD_TEST(lco_newfuture_waitat_full_array_remote);
  ADD_TEST(lco_newfuture_getat_array);
  ADD_TEST(lco_newfuture_getat_array_remote);
  ADD_TEST(lco_newfuture_wait_all);
  ADD_TEST(lco_newfuture_wait_all_remote);
  ADD_TEST(lco_newfuture_get_all);
  ADD_TEST(lco_newfuture_get_all_remote);
  ADD_TEST(lco_newfuture_shared_array);    
});
