//****************************************************************************
// @Filename      13_TestMemput.c
// @Project       High Performance ParallelX Library (libhpx)
//----------------------------------------------------------------------------
// @Subject       Library Unit Test Harness - Memput
// 
// @Compiler      GCC
// @OS            Linux
// @Description   Future based memput test
// @Goal          Goal of this testcase is to test future based memput
// @Copyright     Copyright (c) 2014, Trustees of Indiana University
//                All rights reserved.
//
//                This software may be modified and distributed under the terms
//                of the BSD license.  See the COPYING file for details.
//
//                This software was created at the Indiana University Center 
//                for Research in Extreme Scale Technologies (CREST).
//----------------------------------------------------------------------------
// @Date          09/23/2014
// @Author        Jayashree Candadai <jayaajay [at] indiana.edu>
// @Version       0.1
// Commands to Run: make, mpirun hpxtest 
//****************************************************************************

//****************************************************************************
// @Project Includes
//****************************************************************************
#include "hpx/hpx.h"
#include "tests.h"

static uint64_t block[] = {
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
  22, 23, 24, 25, 26, 27, 28, 29, 30, 31
};

int t13_memput_verify_action(void *args) {
  hpx_addr_t target = hpx_thread_current_target();
  uint64_t *local;
  if (!hpx_gas_try_pin(target, (void**)&local))
    return HPX_RESEND;

  uint64_t result = false;
  const size_t BLOCK_ELEMS = sizeof(block) / sizeof(block[0]);
  for (int i = 0; i < BLOCK_ELEMS; ++i){
    if (local[i] != block[i]) {
      result = true;
    }
  }                 
  hpx_gas_unpin(target);
  hpx_thread_continue(sizeof(uint64_t), &result);
}

//****************************************************************************
// Test code -- for memget
//****************************************************************************
START_TEST (test_libhpx_memput)
{
  printf("Starting the memput test\n");
  int rank = HPX_LOCALITY_ID;
  int size = HPX_LOCALITIES;
  int peerid = (rank+1)%size;
 
  hpx_addr_t data = hpx_gas_global_alloc(size, sizeof(block));
  hpx_addr_t remote = hpx_addr_add(data, peerid * sizeof(block));

  hpx_time_t t1 = hpx_time_now();
  hpx_addr_t localComplete = hpx_lco_future_new(0);
  hpx_addr_t remoteComplete = hpx_lco_future_new(0);
  hpx_gas_memput(remote, block, sizeof(block), localComplete, remoteComplete);
  hpx_lco_wait(localComplete);
  hpx_lco_wait(remoteComplete);
  hpx_lco_delete(localComplete, HPX_NULL);
  hpx_lco_delete(remoteComplete, HPX_NULL);
  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));

  uint64_t output;
  hpx_addr_t done = hpx_lco_future_new(0);
  hpx_call(remote, t13_memput_verify, NULL, 0, done);
  hpx_lco_wait(done);
  
  hpx_lco_get(done, sizeof(output), &output);
  ck_assert_msg(output == false, "gas_memput failed");
  hpx_lco_delete(done, HPX_NULL);
} 
END_TEST

//****************************************************************************
// Register tests from this file
//****************************************************************************
void add_13_TestMemput(TCase *tc) {
  tcase_add_test(tc, test_libhpx_memput);
}
