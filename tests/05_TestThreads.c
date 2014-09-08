//****************************************************************************
// @Filename      05_TestThreads.c
// @Project       High Performance ParallelX Library (libhpx)
//----------------------------------------------------------------------------
// @Subject       Library Unit Test Harness - HPX Thread interface
// 
// @Compiler      GCC
// @OS            Linux
// @Description   Tests the thread functions - thread.h
// @Goal          Goal of this testcase is to test HPX thread interface
//                1. hpx_thread_current_target()             
//                2. hpx_thread_current_cont_target()
//                3. hpx_thread_current_cont_action()
//                4. hpx_thread_current_args_size()
//                5. hpx_thread_yield()
//                6. hpx_thread_get_tls_id()
//                7. hpx_thread_set_affinity()
//                8. hpx_thread_continue()
//                9. hpx_thread_continue_cleanup()
//               10. hpx_thread_exit()                   
//                            
// @Copyright     Copyright (c) 2014, Trustees of Indiana University
//                All rights reserved.
//
//                This software may be modified and distributed under the terms
//                of the BSD license.  See the COPYING file for details.
//
//                This software was created at the Indiana University Center 
//                for Research in Extreme Scale Technologies (CREST).
//----------------------------------------------------------------------------
// @Date          08/22/2014
// @Author        Jayashree Candadai <jayaajay [at] indiana.edu>
// @Version       0.1
// Commands to Run: make, mpirun hpxtest 
//****************************************************************************

//****************************************************************************
// @Project Includes
//****************************************************************************
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "hpx/hpx.h"
#include "tests.h"
#include "domain.h"
#include "libhpx/locality.h"
#include "libsync/queues.h"

#define NUM_THREADS 5
#define ARRAY_SIZE 100

const int DATA_SIZE = sizeof(uint64_t);
const int SET_CONT_VALUE = 1234;

int t05_initData_action(const initBuffer_t *args)
{
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
  uint32_t size = hpx_thread_current_args_size();

  hpx_gas_unpin(local);
  printf("Initialized buffer with index: %u, with message: %s, size of arguments = %d\n", ld->index, ld->message, size);
  return HPX_SUCCESS;
}

//****************************************************************************
// Test code -- ThreadCreate
//****************************************************************************
START_TEST (test_libhpx_threadCreate)
{
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
    hpx_parcel_set_target(p, hpx_addr_add(addr, sizeof(initBuffer_t) * t));
    hpx_parcel_set_action(p, t05_initData);

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
}
END_TEST

//****************************************************************************
// Finish the current thread's execution. The behavior of this call depends
// on the status parameter, and is equivalent to returning status from
// the action.
// HPX_SUCCESS: Normal termination, send a parcel with 0-sized data to the
// the thread's continuation address.
//****************************************************************************
int t05_worker_action(uint64_t *args)
{
  uint64_t n;
  n = *(uint64_t*)args;

  printf("Value of n =  %"PRIu64" \n", n);
  hpx_thread_exit(HPX_LCO_ERROR);
}

START_TEST (test_libhpx_threadExit)
{ 
  printf("Starting the Thread Exit test\n");
  // Start the timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t done = hpx_lco_future_new(sizeof(uint64_t));
  uint64_t value = SET_CONT_VALUE;
  hpx_status_t status = hpx_call(HPX_HERE, t05_worker, &value, sizeof(value),
                                 done);
  ck_assert_msg(status == HPX_SUCCESS, "Could not normally terminate the thread");
  hpx_lco_wait(done);

  hpx_lco_get(done, sizeof(uint64_t), &value);
  ck_assert(value == HPX_SUCCESS);

  hpx_lco_delete(done, HPX_NULL);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
}
END_TEST


//****************************************************************************
// hpx_thread_get_tls_id() Generates a consecutive new ID for a thread
// The first time this is called in a leightweight thread, it assigns the next
// available ID. Each time it's called after that it returns the same ID.
//****************************************************************************
int t05_assignID_action(void *args)
{
  int tid = hpx_thread_get_tls_id();
  int consecutiveID = hpx_thread_get_tls_id();
  printf("First time generated ID: %d, consecutive new ID:  %d\n", tid, 
                                       consecutiveID);
  return HPX_SUCCESS;
}

START_TEST (test_libhpx_threadGetTlsID)
{
  printf("Starting the Threads ID generation test\n");
  // Start the timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t done = hpx_lco_and_new(NUM_THREADS);

  // HPX Threads are spawned as a result of hpx_parcel_send() / hpx_parcel_
  // sync(). 
  for (int t = 0; t < NUM_THREADS; t++) {
    hpx_parcel_t *p = hpx_parcel_acquire(NULL, 0);

    // Set the target address and action for the parcel
    hpx_parcel_set_target(p, HPX_THERE(t % hpx_get_num_ranks()));
    hpx_parcel_set_action(p, t05_assignID);

    // Set the continuation target and action for parcel
    hpx_parcel_set_cont_target(p, done);
    hpx_parcel_set_cont_action(p, hpx_lco_set_action);

    // and send the parcel, this spawns the HPX thread
    hpx_parcel_send(p, HPX_NULL);
  }

  hpx_lco_wait(done);

  hpx_lco_delete(done, HPX_NULL);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
}
END_TEST

//****************************************************************************
// Finish the current thread's execution, sending value to the thread's
// continuation address (size is the size of the value and value is the value
// to be sent to the thread's continuation address.
//****************************************************************************

int t05_set_cont_action(void *args) {
  uint64_t value = SET_CONT_VALUE;
  hpx_thread_continue(DATA_SIZE, &value);
}

START_TEST (test_libhpx_threadContinue)
{
  printf("Starting the Thread continue test\n");
  // Start the timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t *cont_fut = malloc(sizeof(hpx_addr_t) * hpx_get_num_ranks());


  for (int i = 0; i < hpx_get_num_ranks(); i++) { 
    cont_fut[i] = hpx_lco_future_new(DATA_SIZE);
    hpx_parcel_t *p = hpx_parcel_acquire(NULL, 0);
    hpx_parcel_set_target(p, HPX_THERE(i));
    hpx_parcel_set_action(p, t05_cont_thread);
    hpx_parcel_set_cont_target(p, cont_fut[i]);
    hpx_parcel_set_cont_action(p, hpx_lco_set_action);
    hpx_parcel_send(p, HPX_NULL); 
    printf("Sending action with continuation to %d\n", i);
  }

  for (int i = 0; i < hpx_get_num_ranks(); i++) {
    uint64_t result;
    printf("Waiting on continuation to %d\n", i);
    hpx_lco_get(cont_fut[i], DATA_SIZE, &result);
    printf("Received continuation from %d with value %" PRIu64 "\n", i, result);
    assert(result == SET_CONT_VALUE);
  }

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
}
END_TEST

//****************************************************************************
// hpx_thread_yield()
//****************************************************************************

struct t05_thread_yield_args {
  int iters;
  two_lock_queue_t *q;
  hpx_addr_t consumer_ready;
  hpx_addr_t producer_ready;
};

int t05_thread_yield_consumer_action(void *vargs) {
  hpx_thread_set_affinity(hpx_get_num_threads() - 1);

  struct t05_thread_yield_args *args = (struct t05_thread_yield_args*)vargs;
  hpx_lco_set(args->consumer_ready, 0, NULL, HPX_NULL, HPX_NULL);
  hpx_lco_wait(args->producer_ready);

  sync_two_lock_queue_dequeue(args->q);
  hpx_thread_yield();
  sync_two_lock_queue_dequeue(args->q);

  return HPX_SUCCESS;
}

int t05_thread_yield_producer_action(void *vargs) {
  // set affinity
  hpx_thread_set_affinity(hpx_get_num_threads() - 1);
  
  // initialize data
  int old_head_value = -1;
  int new_head_value = -1;
  hpx_addr_t done = hpx_lco_future_new(0);
  struct t05_thread_yield_args args = {
    .iters = 4,
    .q = sync_two_lock_queue_new(),
    .consumer_ready = hpx_lco_future_new(0),
    .producer_ready = hpx_lco_future_new(0)
  };
  int *numbers = malloc(sizeof(int) * args.iters);

  // create consumer
  hpx_call(HPX_HERE, t05_thread_yield_consumer, &args, sizeof(args), done);

  // synchronize
  hpx_lco_wait(args.consumer_ready);
  hpx_lco_set(args.producer_ready, 0, NULL, HPX_NULL, HPX_NULL);

  // produce
  for (int i = 0; i < args.iters; i++) {
    numbers[i] = i;
    sync_two_lock_queue_enqueue(args.q, &numbers[i]);
  }

  // check data pre-yield
  void *head_value = sync_two_lock_queue_dequeue(args.q); // we expect this to be 0
  if (head_value != NULL)
    old_head_value = *(int*)head_value;
  ck_assert_msg(old_head_value == 0, "Queue head contained unexpected value. Either (1) thread_affinity() failed or (2) thread scheduling assumptions are incorrect.");
  // new head should now be 1

  hpx_thread_yield();
  // new head should now be 2

  // check data post-yield
  head_value = sync_two_lock_queue_dequeue(args.q); // we expect this to be 2
  if (head_value != NULL)
    new_head_value = *(int*)head_value;
  ck_assert_msg(new_head_value == 2, "Thread did not yield.");
  printf("new head value == %d\n", new_head_value);

  // cleanup
  hpx_lco_wait(done);
  sync_two_lock_queue_delete(args.q);
  free(numbers);

  return HPX_SUCCESS;
}

START_TEST (test_libhpx_threadYield)
{
  int retval = hpx_call_sync(HPX_HERE, t05_thread_yield_producer, NULL, 0, NULL, 0);
  ck_assert(retval == HPX_SUCCESS);
}
END_TEST


//****************************************************************************
// Finish the current thread's execution, sending value to the thread's
// continuation address (size is the size of the value and value is the value
// to be sent to the thread's continuation address. This version gives the 
// application a chance to cleanup for instance, to free the value. After
// dealing with the continued data, it will run cleanup(env).
//****************************************************************************
int t05_thread_cont_cleanup_action(void *args) {
  hpx_addr_t addr = hpx_thread_current_target();
  uint64_t local;
  if (!hpx_gas_try_pin(addr, (void**)&local))
    return HPX_RESEND;

  local = SET_CONT_VALUE;
  uint64_t *value = (uint64_t*) malloc(sizeof(uint64_t));
  *value = local;

  hpx_gas_unpin(addr);
  hpx_thread_continue_cleanup(DATA_SIZE, value, free, value);
}

START_TEST (test_libhpx_threadContinueCleanup)
{
  printf("Starting the Thread continue cleanup test\n");
  // Start the timer
  hpx_time_t t1 = hpx_time_now();
  
  hpx_addr_t src = hpx_gas_alloc(sizeof(uint64_t));
  int rank = hpx_get_my_rank();

  uint64_t *block = malloc(DATA_SIZE);
  assert(block);

  hpx_call_sync(src, t05_thread_cont_cleanup, &rank, sizeof(rank), block, DATA_SIZE);
  printf("value in block is %"PRIu64"\n", *block);

  hpx_gas_free(src, HPX_NULL);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
}
END_TEST

//****************************************************************************
// hpx_thread_current_cont_action gets the continuation action for the current
// thread
//****************************************************************************
int t05_thread_current_cont_target_action(void *args) {
  hpx_addr_t addr = hpx_thread_current_target();
  uint64_t local;
  if (!hpx_gas_try_pin(addr, (void**)&local))
    return HPX_RESEND;

  hpx_action_t c_action = hpx_thread_current_cont_action();
  hpx_addr_t   c_target = hpx_thread_current_cont_target();
  
  local = SET_CONT_VALUE;

  hpx_addr_t done = hpx_lco_future_new(0);
  hpx_call(c_target, c_action, &local, DATA_SIZE, done);
  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);

  hpx_gas_unpin(addr);
  return HPX_SUCCESS;
}

START_TEST (test_libhpx_threadContAction)
{
  printf("Starting the Thread continue target and action test\n");
  // Start the timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t *cont_fut = malloc(sizeof(hpx_addr_t) * hpx_get_num_ranks());

  for (int i = 0; i < hpx_get_num_ranks(); i++) {
    cont_fut[i] = hpx_lco_future_new(DATA_SIZE);
    hpx_parcel_t *p = hpx_parcel_acquire(NULL, DATA_SIZE);
    hpx_parcel_set_target(p, HPX_THERE(i));
    hpx_parcel_set_action(p, t05_thread_current_cont_target);
    hpx_parcel_set_cont_target(p, cont_fut[i]);
    hpx_parcel_set_cont_action(p, hpx_lco_set_action);
    hpx_parcel_send(p, HPX_NULL);
  }

  for (int i = 0; i < hpx_get_num_ranks(); i++) {
    uint64_t result;
    hpx_lco_get(cont_fut[i], DATA_SIZE, &result);
    printf("Received continuation from %d with value %" PRIu64 "\n", i, result);
    assert(result == SET_CONT_VALUE);
  }

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
}
END_TEST

//****************************************************************************
// Register tests from this file
//****************************************************************************
void add_05_TestThreads(TCase *tc) {
  tcase_add_test(tc, test_libhpx_threadCreate);
  tcase_add_test(tc, test_libhpx_threadExit);
  tcase_add_test(tc, test_libhpx_threadGetTlsID);
  tcase_add_test(tc, test_libhpx_threadContinue);
  tcase_add_test(tc, test_libhpx_threadContinueCleanup);
  tcase_add_test(tc, test_libhpx_threadContAction);
  tcase_add_test(tc, test_libhpx_threadYield);
}
