//****************************************************************************
// @Filename      12_TestMemGet.c
// @Project       High Performance ParallelX Library (libhpx)
//----------------------------------------------------------------------------
// @Subject       Library Unit Test Harness - Memget
// 
// @Compiler      GCC
// @OS            Linux
// @Description   Future based memget test
// @Goal          Goal of this testcase is to test future based memget 
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

#define ARRAY_SIZE 32

int t12_init_array_action(size_t* args) {
  size_t n = *args;
  hpx_addr_t target = hpx_thread_current_target();
  uint64_t *local = NULL;
  if (!hpx_gas_try_pin(target, (void**)&local))
    return HPX_RESEND;

  for(int i = 0; i < n; i++)
    local[i] = i;
  HPX_THREAD_CONTINUE(local);
}

//****************************************************************************
// Test code -- for memget
//****************************************************************************
START_TEST (test_libhpx_memget)
{
  printf("Starting the memget test\n");
  int rank = HPX_LOCALITY_ID;
  int size = HPX_LOCALITIES;
  int peerid = (rank+1)%size;
  uint64_t *local;
 
  hpx_addr_t data = hpx_gas_global_alloc(size, ARRAY_SIZE*sizeof(uint64_t));
  hpx_addr_t remote = hpx_addr_add(data, ARRAY_SIZE*sizeof(uint64_t)* peerid);

  size_t arraysize = ARRAY_SIZE;
  hpx_addr_t done = hpx_lco_future_new(sizeof(void*));
  hpx_call(remote, t12_init_array, &arraysize, sizeof(arraysize), done);
  hpx_call_sync(data, t12_init_array, &arraysize, sizeof(arraysize), &local, sizeof(local));
  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);

  // allocate and start a timer
  uint64_t localBuf[arraysize];
  hpx_time_t t1 = hpx_time_now();
  hpx_addr_t completed = hpx_lco_future_new(0);
  hpx_gas_memget(&localBuf, remote, arraysize*sizeof(uint64_t), completed);
  hpx_lco_wait(completed);
  hpx_lco_delete(completed, HPX_NULL);
  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));

  for (int i = 0; i < arraysize; i++)
    printf("%"PRIu64"", localBuf[i]);
  printf("\n");
} 
END_TEST

//****************************************************************************
// Register tests from this file
//****************************************************************************
void add_12_TestMemget(TCase *tc) {
  tcase_add_test(tc, test_libhpx_memget);
}
