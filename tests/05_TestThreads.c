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

#define NUM_THREADS 5
#define ARRAY_SIZE 100

static int t05_data_move = 0;

int t05_initData_action(const InitBuffer *args)
{
 // Get the target of the current thread. The target of the thread is the
 // destination that a parcel was sent to to spawn the current thread.
 // hpx_thread_current_target() returns the address of the thread's target
  hpx_addr_t local = hpx_thread_current_target();
  InitBuffer *ld = NULL;
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

  hpx_addr_t addr = hpx_gas_global_alloc(NUM_THREADS, sizeof(InitBuffer));
  hpx_addr_t done = hpx_lco_and_new(NUM_THREADS);
  
  // HPX Threads are spawned as a result of hpx_parcel_send() / hpx_parcel_
  // sync(). 
  for (int t = 0; t < NUM_THREADS; t++) {
    hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(InitBuffer));
    
    // Fill the buffer
    InitBuffer *init = hpx_parcel_get_data(p);
    init->index = t;
    strcpy(init->message, "Thread creation test");

    // Set the target address and action for the parcel
    hpx_parcel_set_target(p, hpx_addr_add(addr, sizeof(InitBuffer) * t));
    hpx_parcel_set_action(p, t05_initData);

    // Set the continuation target and action for parcel
    hpx_parcel_set_cont_target(p, done);
    hpx_parcel_set_cont_action(p, hpx_lco_set_action);

    // and send the parcel, this spawns the HPX thread
    hpx_parcel_send(p, HPX_NULL);
  }

  hpx_lco_wait(done);
  
  hpx_lco_delete(done, HPX_NULL);
  hpx_gas_global_free(addr, HPX_NULL);

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
int t05_worker_action(int *args)
{
  hpx_addr_t local = hpx_thread_current_target();
  int n;
  if (!hpx_gas_try_pin(local, (void**)&n))
    return HPX_RESEND;
  n = *(int*)args;

  printf("Value of n =  %d\n", n);
  hpx_thread_exit(HPX_SUCCESS);
}

START_TEST (test_libhpx_threadExit)
{ 
  printf("Starting the Thread Exit test\n");
  // Start the timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t done = hpx_lco_future_new(0);
  int value = 1000;
  hpx_status_t status = hpx_call(HPX_HERE, t05_worker, &value, sizeof(value),
                                 done);
  ck_assert_msg(status == HPX_SUCCESS, "Could not normally terminate the thread");
  hpx_lco_wait(done);
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
  //ck_assert_msg(tid == consecutiveID, "Error in expected behavior of generation of thread ID"); 
  printf("First time generated ID: %d, consecutive new ID:  %d\n", tid, 
                                       consecutiveID);
  return HPX_SUCCESS;
}

START_TEST (test_libhpx_threadGetTlsID)
{
  printf("Starting the Threads ID generation test\n");
  // Start the timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t addr = hpx_gas_global_alloc(NUM_THREADS, sizeof(int));
  hpx_addr_t done = hpx_lco_and_new(NUM_THREADS);

  // HPX Threads are spawned as a result of hpx_parcel_send() / hpx_parcel_
  // sync(). 
  for (int t = 0; t < NUM_THREADS; t++) {
    hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(int));

    // Set the target address and action for the parcel
    hpx_parcel_set_target(p, hpx_addr_add(addr, sizeof(int) * t));
    hpx_parcel_set_action(p, t05_assignID);

    // Set the continuation target and action for parcel
    hpx_parcel_set_cont_target(p, done);
    hpx_parcel_set_cont_action(p, hpx_lco_set_action);

    // and send the parcel, this spawns the HPX thread
    hpx_parcel_send(p, HPX_NULL);
  }

  hpx_lco_wait(done);

  hpx_lco_delete(done, HPX_NULL);
  hpx_gas_global_free(addr, HPX_NULL);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
}
END_TEST

//****************************************************************************
// Finish the current thread's execution, sending value to the thread's
// continuation address (size is the size of the value and value is the value
// to be sent to the thread's continuation address.
//****************************************************************************
int t05_memput_action(void *args) {
  hpx_addr_t target = hpx_thread_current_target();
  char *local;
  if (!hpx_gas_try_pin(target, (void**)&local))
    return HPX_RESEND;

  memcpy(local, args, hpx_thread_current_args_size());
  hpx_gas_unpin(target);
  return HPX_SUCCESS;
}

int t05_memget_action(size_t *args) {
  size_t n = *args;
  hpx_addr_t target = hpx_thread_current_target();
  char *local;
  if (!hpx_gas_try_pin(target, (void**)&local))
    return HPX_RESEND;

  hpx_gas_unpin(target);
  hpx_thread_continue(n, local);
}

// array get is synchronous and returns the value
uint64_t array_get(hpx_addr_t array, long i) {
  uint64_t val;
  size_t n = sizeof(val);
  hpx_addr_t there = hpx_addr_add(array, i*n);
  hpx_call_sync(there, t05_memget, &n, sizeof(n), &val, n);
  return val;
}

// array set is asynchronous and uses an LCO for synchronization.
void array_set(hpx_addr_t array, long i, uint64_t val,
               hpx_addr_t done) {
  hpx_addr_t there = hpx_addr_add(array, i*sizeof(uint64_t));
  hpx_call(there, t05_memput, &val, sizeof(val), done);
}

// Initialize the array
int t05_init_array_action(contTest_config_t *cfg) {
  hpx_addr_t target = hpx_thread_current_target();
  uint64_t *local;
  if (!hpx_gas_try_pin(target, (void**)&local))
    return HPX_RESEND;

  int me = HPX_LOCALITY_ID;
  int nranks = HPX_LOCALITIES;
  long r = cfg->arraySize % nranks;
  long blocks = cfg->arraySize / nranks + ((me < r) ? 1 : 0);
  hpx_addr_t done = hpx_lco_and_new(blocks);
  for (long b = 0, i = me; b < blocks; ++b, i += nranks) {
    array_set(cfg->array, i, i, done);
    //printf("Initializing the array at %ld, with value %ld\n", i, i);
  }

  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);
  hpx_gas_unpin(target);
  return HPX_SUCCESS;
}

int t05_update_array_action(contTest_config_t *cfg) {
  uint64_t val[ARRAY_SIZE];
  hpx_addr_t completed;

  // Update the array with random number
  for (int i = 0; i < cfg->arraySize; i++) 
    val[i] = array_get(cfg->array, i);
  
  completed = hpx_lco_and_new(ARRAY_SIZE);
  for (int i = 0; i < cfg->arraySize; i++) {
    uint64_t randVal =  rand() % ARRAY_SIZE;
    array_set(cfg->array, i, randVal, completed);
    //printf("Updating the array at %d, with value %ld\n", i, randVal); 
  }  
  hpx_lco_wait(completed);
  hpx_lco_delete(completed, HPX_NULL);
  
  return HPX_SUCCESS;
}

int t05_threadContMain_action(contTest_config_t *cfg) {
  printf("nThreads = %d\n", hpx_get_num_ranks());
  fflush(stdout);

  // Allocate main array
  cfg->array = hpx_gas_global_alloc(ARRAY_SIZE, sizeof(uint64_t));

  // Initialize the array
  hpx_addr_t done = hpx_lco_future_new(0);
  hpx_bcast(t05_init_array, cfg, sizeof(*cfg), done);
  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);

  printf("Initialization of array complete.\n");
  fflush(stdout);

  // Update the array
  done = hpx_lco_future_new(0);
  hpx_bcast(t05_update_array, cfg, sizeof(*cfg), done);
  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);

  printf("Completed array updates\n");
  fflush(stdout);
  hpx_thread_exit(HPX_SUCCESS);
}

START_TEST (test_libhpx_threadContinue)
{
  printf("Starting the Thread continue test\n");

  contTest_config_t contTest_cfg  = {
    .arraySize  = ARRAY_SIZE,
    .array      = HPX_NULL,
  };

  // Start the timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t done = hpx_lco_future_new(0);
  hpx_status_t status = hpx_call(HPX_HERE, t05_threadContMain, 
                                 &contTest_cfg, sizeof(contTest_cfg),
                                 done);
  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);

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
}
