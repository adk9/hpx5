//****************************************************************************
// @Filename      07_TestLCO.c
// @Project       High Performance ParallelX Library (libhpx)
//----------------------------------------------------------------------------
// @Subject       Library Unit Test Harness - HPX LCO Interface
// 
// @Compiler      GCC
// @OS            Linux
// @Description   lco.h File Reference
// @Goal          Goal of this testcase is to test the HPX LCO Functions
//                1. hpx_lco_delete
//                2. hpx_lco_set
//                3. hpx_lco_wait
//                4. hpx_lco_get
//                5. hpx_lco_wait_all
//                6. hpx_lco_get_all
//                7. hpx_lco_error
// @Copyright     Copyright (c) 2014, Trustees of Indiana University
//                All rights reserved.
//
//                This software may be modified and distributed under the terms
//                of the BSD license.  See the COPYING file for details.
//
//                This software was created at the Indiana University Center 
//                for Research in Extreme Scale Technologies (CREST).
//----------------------------------------------------------------------------
// @Date          09/03/2014
// @Author        Jayashree Candadai <jayaajay [at] indiana.edu>
// @Version       0.1
// Commands to Run: make, mpirun hpxtest 
//****************************************************************************

//****************************************************************************
// @Project Includes
//****************************************************************************
#include "hpx/hpx.h"
#include "tests.h"
#include "domain.h"

//****************************************************************************
// Testcase to test LCO wait and delete functions
//****************************************************************************
int t07_init_array_action(size_t *args) {
  size_t n = *args;
  hpx_addr_t target = hpx_thread_current_target();
  char *local;
  if (!hpx_gas_try_pin(target, (void**)&local))
    return HPX_RESEND;

  for(int i = 0; i < n; i++) 
    local[i] = (HPX_LOCALITY_ID == 0) ? 'a' : 'b';

  HPX_THREAD_CONTINUE(local); 
}

START_TEST (test_libhpx_lcoFunction)
{
  int size = HPX_LOCALITIES;
  int peerID = (HPX_LOCALITY_ID + 1) % size;
 
  fprintf(test_log, "Starting the HPX LCO test\n");
  // Start the timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t addr = hpx_gas_global_alloc(size, BUFFER_SIZE*2);
  hpx_addr_t remote = hpx_addr_add(addr, BUFFER_SIZE*2 * peerID, BUFFER_SIZE*2);

  hpx_addr_t done;
  
  for (size_t i = 1; i <= BUFFER_SIZE; i*=2) {
    char *local;
    // Create a future. Futures are builtin LCO's that represent from async
    // computation. Futures are allocated in the global address space.
    done = hpx_lco_future_new(sizeof(void*));
    hpx_call(remote, t07_init_array, done, &i, sizeof(i));
    hpx_call_sync(addr, t07_init_array, &local, sizeof(local), &i, sizeof(i));
    
    // Perform a wait operation. The LCO blocks the caller until an LCO set
    // operation triggers the LCO. 
    hpx_status_t status = hpx_lco_wait(done);
    ck_assert(status == HPX_SUCCESS);

    // Deletes an LCO. It takes address of the LCO and an rsync ot signal
    // remote completion.
    hpx_lco_delete(done, HPX_NULL);
  }
  
  hpx_gas_free(addr, HPX_NULL);
  fprintf(test_log, " Elapsed: %g\n", hpx_time_elapsed_ms(t1));
}
END_TEST

//****************************************************************************
// Testcase to test hpx_lco_set and hpx_lco_get functions.
//****************************************************************************
int t07_lcoSetGet_action(uint64_t *args) {
  hpx_addr_t future = hpx_lco_future_new(sizeof(uint64_t));

  uint64_t val = 1234;
  // Set an LCO, with data.
  hpx_lco_set(future, sizeof(uint64_t), &val, HPX_NULL, HPX_NULL);

  // Get the value and print for debugging purpose. An LCO blocks the caller
  // until the future is set, and then copies its value to data into the 
  // provided output location.
  uint64_t setVal;
  hpx_lco_get(future, sizeof(setVal), &setVal);
  hpx_lco_delete(future, HPX_NULL);

  //printf("Value set is = %"PRIu64"\n", setVal);
  hpx_thread_continue(sizeof(uint64_t), &setVal);
}

START_TEST (test_libhpx_lcoSetGet) 
{
  int size = HPX_LOCALITIES;
  uint64_t n = 0;
  
  fprintf(test_log, "Starting the HPX LCO Set and Get test\n");
  fprintf(test_log, "localities: %d\n", size);
  // Start the timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t done = hpx_lco_future_new(sizeof(uint64_t));
  hpx_call(HPX_HERE, t07_lcoSetGet, done, &n, sizeof(n));

  uint64_t result;
  hpx_lco_get(done, sizeof(uint64_t), &result);
  fprintf(test_log, "Value returned = %"PRIu64"\n", result);
  fprintf(test_log, " Elapsed: %g\n", hpx_time_elapsed_ms(t1));

  hpx_lco_delete(done, HPX_NULL);
}
END_TEST

//****************************************************************************
// Testcase to test hpx_lco_wait_all function.
//****************************************************************************
int t07_initBlock_action(uint32_t *args) 
{
  hpx_addr_t target = hpx_thread_current_target();
  uint32_t *buffer = NULL;
  if (!hpx_gas_try_pin(target, (void**)&buffer))
    return HPX_RESEND;

  uint32_t block_size = args[0];
  for (int i = 0; i < block_size; i++){
    // Initialixe the buffer
    buffer[i] = 1234;
    //printf("Initializing the buffer for locality ID %d: %d\n", 
    //       HPX_LOCALITY_ID, buffer[i]);
  } 
  hpx_gas_unpin(target);
  return HPX_SUCCESS;
}

int t07_initMemory_action(uint32_t *args) 
{
  hpx_addr_t local = hpx_thread_current_target();
  uint32_t block_size = args[0];
  uint32_t block_bytes = block_size * sizeof(uint32_t);
  uint32_t blocks = args[1];

  hpx_addr_t completed = hpx_lco_and_new(blocks);
  for (int i = 0; i < blocks; i++) {
    hpx_addr_t block = hpx_addr_add(local, i * HPX_LOCALITY_ID * block_bytes, block_bytes);
    hpx_call(block, t07_initBlock, completed, args, 2 * sizeof(*args));
  }
  hpx_lco_wait(completed);
  hpx_lco_delete(completed, HPX_NULL);
  return HPX_SUCCESS;
}

START_TEST (test_libhpx_lcoWaitAll)
{
  int size = HPX_LOCALITIES;
  int block_size = 1;
  int ranks = hpx_get_num_ranks();

  fprintf(test_log, "Starting the HPX LCO Wait all test\n");
  fprintf(test_log, "localities: %d\n", size);

  // Start the timer
  hpx_time_t t1 = hpx_time_now();

  uint32_t blocks = size;
  uint32_t block_bytes = block_size * sizeof(uint32_t);

  fprintf(test_log,"Number of blocks and bytes per block = %d, %d\n", blocks, block_bytes);
  fprintf(test_log, "Ranks and blocks per rank = %d, %d\n", ranks, blocks / ranks);
  hpx_addr_t addr = hpx_gas_global_alloc(blocks, block_bytes);

  uint32_t args[2] = {
    block_size,
    (blocks / ranks)
  };

  int rem = blocks % ranks;
  hpx_addr_t done[2] = {
    hpx_lco_and_new(ranks),
    hpx_lco_and_new(rem)
  };

  for (int i = 0; i < ranks; i++) {
    hpx_addr_t there = hpx_addr_add(addr, i * block_bytes, block_bytes);
    hpx_call(there, t07_initMemory, done[0], args, sizeof(args));
  }

  for (int i = 0; i < rem; i++) {
    hpx_addr_t block = hpx_addr_add(addr, args[1] * ranks + i * block_bytes, block_bytes);
    hpx_call(block, t07_initMemory, done[1], args, sizeof(args[0]));
  }

  // Blocks the thread until all of the LCO's have been set.
  hpx_lco_wait_all(2, done, NULL);
  hpx_lco_delete(done[0], HPX_NULL);
  hpx_lco_delete(done[1], HPX_NULL);
  
  hpx_gas_free(addr, HPX_NULL);

  fprintf(test_log, " Elapsed: %g\n", hpx_time_elapsed_ms(t1));
}
END_TEST

//****************************************************************************
// Testcase to test hpx_lco_get_all function
//****************************************************************************

int t07_getAll_action(uint32_t *args) {
  uint32_t n = *args;
  if (n < 2)
    HPX_THREAD_CONTINUE(n);

  hpx_addr_t peers[] = {
    HPX_HERE,
    HPX_HERE
  };

  uint32_t ns[] = {
    n - 1,
    n - 2
  };

  hpx_addr_t futures[] =  {
    hpx_lco_future_new(sizeof(uint32_t)),
    hpx_lco_future_new(sizeof(uint32_t))
  };

  uint32_t ssn[] = {
    0,
    0
  };

  void *addrs[] = {
    &ssn[0],
    &ssn[1]
  };

  int sizes[] = {
    sizeof(uint32_t),
    sizeof(uint32_t)
  };

  hpx_call(peers[0], t07_getAll, futures[0], &ns[0], sizeof(uint32_t));
  hpx_call(peers[1], t07_getAll, futures[1], &ns[1], sizeof(uint32_t));

  hpx_lco_get_all(2, futures, sizes, addrs, NULL);

  hpx_lco_wait(futures[0]);
  hpx_lco_wait(futures[1]);

  hpx_lco_delete(futures[0], HPX_NULL);
  hpx_lco_delete(futures[1], HPX_NULL);

  uint32_t sn = ssn[0] * ssn[0] + ssn[1] * ssn[1]; 

  HPX_THREAD_CONTINUE(sn);
  return HPX_SUCCESS;
}

START_TEST (test_libhpx_lcoGetAll) 
{
  uint32_t n, ssn;
  fprintf(test_log, "Starting the HPX LCO get all test\n");
  for (uint32_t i = 0; i < 6; i++) {
    ssn = 0;
    n = i + 1;
    hpx_time_t t1 = hpx_time_now();
    fprintf(test_log, "Square series for (%d): ", n);
    fflush(stdout);
    hpx_call_sync(HPX_HERE, t07_getAll, &ssn, sizeof(ssn), &n, sizeof(n));
    fprintf(test_log,"%d", ssn);
    fprintf(test_log, " Elapsed: %.7f\n", hpx_time_elapsed_ms(t1)/1e3);
  }
}
END_TEST

//****************************************************************************
// Testcase to test hpx_lco_error function
//****************************************************************************
int t07_errorSet_action(void *args) {
  hpx_addr_t addr = *(hpx_addr_t*)args;
  // Propagate an error to an LCO
  hpx_lco_error(addr, HPX_ERROR, HPX_NULL);
  return HPX_SUCCESS;
}

START_TEST (test_libhpx_lcoError) 
{
  fprintf(test_log, "Starting the HPX LCO get all test\n");
  hpx_time_t t1 = hpx_time_now();
  hpx_addr_t lco = hpx_lco_future_new(0);
  hpx_addr_t done = hpx_lco_future_new(0);
  hpx_call(HPX_HERE, t07_errorSet, done, &lco, sizeof(lco));
  hpx_status_t status = hpx_lco_wait(lco);
  fprintf(test_log, "status == %d\n", status);
  ck_assert(status == HPX_ERROR);
  hpx_lco_wait(done);

  hpx_lco_delete(lco, HPX_NULL);
  hpx_lco_delete(done, HPX_NULL);
 
  fprintf(test_log, " Elapsed: %.7f\n", hpx_time_elapsed_ms(t1)/1e3);
}
END_TEST

//****************************************************************************
// Register tests from this file
//****************************************************************************

void add_07_TestLCO(TCase *tc) {
  tcase_add_test(tc, test_libhpx_lcoFunction);
  tcase_add_test(tc, test_libhpx_lcoSetGet);
  tcase_add_test(tc, test_libhpx_lcoWaitAll);
  tcase_add_test(tc, test_libhpx_lcoGetAll);
  tcase_add_test(tc, test_libhpx_lcoError);
}
