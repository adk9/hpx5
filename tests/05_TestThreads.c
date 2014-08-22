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

int t05_initData_action(const InitBuffer *args)
{
  hpx_addr_t local = hpx_thread_current_target();
  InitBuffer *ld = NULL;
  if (!hpx_gas_try_pin(local, (void**)&ld))
    return HPX_RESEND;

  ld->threadNo = args->threadNo;
  strcpy(ld->message, args->message);

  hpx_gas_unpin(local);
  printf("Initialized thread ID: %u, with message %s\n", ld->threadNo, ld->message);
  return HPX_SUCCESS;
}

//****************************************************************************
// Test code -- ThreadCreate
//****************************************************************************
START_TEST (test_libhpx_threadCreate)
{
  int threads[NUM_THREADS];
  
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
    init->threadNo = t;
    strcpy(init->message, "Starting threads test");

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
// Register tests from this file
//****************************************************************************

void add_05_TestThreads(TCase *tc) {
  tcase_add_test(tc, test_libhpx_threadCreate);
}
