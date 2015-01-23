//****************************************************************************
// @Filename      08_TestSema.c
// @Project       High Performance ParallelX Library (libhpx)
//----------------------------------------------------------------------------
// @Subject       Library Unit Test Harness - Semaphores
// 
// @Compiler      GCC
// @OS            Linux
// @Description   sema.c File Reference
// @Goal          Goal of this testcase is to test the HPX LCO Semaphores
//                1. hpx_lco_sema_new -- Create new semaphore
//                2. hpx_lco_sema_p -- Standard semaphore P (Wait) operation.
//                3. hpx_lco_sema_v -- Standard semaphore V (Signal) operation.
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
// @Date          09/04/2014
// @Author        Jayashree Candadai <jayaajay [at] indiana.edu>
// @Version       0.1
// Commands to Run: make, mpirun hpxtest 
//****************************************************************************

//****************************************************************************
// @Project Includes
//****************************************************************************
#include "hpx/hpx.h"
#include "tests.h"

hpx_addr_t mutex;
int counter; /* shared variable */

int t08_handler_action(uint32_t *args) {
  uint32_t x = *args;
  fprintf(test_log, "Thread %d: Waiting to enter critical region...\n", x);

  // Standard semaphore P (wait) operation.
  // Attempts to decrement the count in the semaphore; block
  // if the count is 0
  hpx_lco_sema_p(mutex);

  /* START CRITICAL REGION */
  fprintf(test_log, "Thread %d: Now in critical region...\n", x);
  fprintf(test_log, "Thread %d: Counter Value: %d\n", x, counter);
  fprintf(test_log, "Thread %d: Incrementing Counter...\n", x);
  counter++;
  fprintf(test_log, "Thread %d: New Counter Value: %d\n", x, counter);
  fprintf(test_log, "Thread %d: Exiting critical region...\n", x);
  /* END CRITICAL REGION */

  // Standard semaphore V (Signal) operation. 
  // increments the count in the semaphore, signaling the LCO if the 
  // increment transitions from 0 to 1. This is always async.    
  hpx_lco_sema_v(mutex);       /* up semaphore */

  return HPX_SUCCESS;
}


//****************************************************************************
// Test code -- for HPX LCO Semaphores
//****************************************************************************
START_TEST (test_libhpx_lco_Semaphores)
{
  hpx_addr_t peers[] = {HPX_HERE, HPX_HERE};
  uint32_t i[] = {0, 1};
  int sizes[] = {sizeof(uint32_t), sizeof(uint32_t)};
  uint32_t array[] = {0, 0};
  void *addrs[] = {&array[0], &array[1]};

  hpx_addr_t futures[] = {
    hpx_lco_future_new(sizeof(uint32_t)),
    hpx_lco_future_new(sizeof(uint32_t))
  };

  fprintf(test_log, "Starting the HPX LCO Semaphore test\n");
  hpx_time_t t1 = hpx_time_now();

  // create a new semaphore
  // Returns the global address
  // initial value this semaphore would be created with
  mutex = hpx_lco_sema_new(1);

  hpx_call(peers[0], t08_handler, futures[0], &i[0], sizeof(uint32_t));
  hpx_call(peers[1], t08_handler, futures[1], &i[1], sizeof(uint32_t));
    
  hpx_lco_get_all(2, futures, sizes, addrs, NULL);
   
  hpx_lco_delete(futures[0], HPX_NULL);
  hpx_lco_delete(futures[1], HPX_NULL);

  fprintf(test_log, " Elapsed: %g\n", hpx_time_elapsed_ms(t1));
} 
END_TEST

//****************************************************************************
// Register tests from this file
//****************************************************************************
void add_08_TestSemaphores(TCase *tc) {
  tcase_add_test(tc, test_libhpx_lco_Semaphores);
}
