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

static uint64_t block[] = {
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
  22, 23, 24, 25, 26, 27, 28, 29, 30, 31
};


int t12_init_array_action(void* args) {
  hpx_addr_t target = hpx_thread_current_target();
  uint64_t *local = NULL;
  if (!hpx_gas_try_pin(target, (void**)&local))
    return HPX_RESEND;

  memcpy(local, args, hpx_thread_current_args_size());
  hpx_gas_unpin(target);
  return HPX_SUCCESS;
}

//****************************************************************************
// Test code -- for memget
//****************************************************************************
START_TEST (test_libhpx_memget)
{
  fprintf(test_log, "Starting the memget test\n");
  int rank = HPX_LOCALITY_ID;
  int size = HPX_LOCALITIES;
  int peerid = (rank+1)%size;

  hpx_addr_t data = hpx_gas_global_alloc(size, sizeof(block));
  hpx_addr_t remote = hpx_addr_add(data, peerid * sizeof(block), sizeof(block));

  hpx_addr_t done = hpx_lco_future_new(sizeof(void*));
  hpx_call(remote, t12_init_array, done, block, sizeof(block));
  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);

  const size_t BLOCK_ELEMS = sizeof(block) / sizeof(block[0]);
  uint64_t local[BLOCK_ELEMS];
  memset(&local, 0xFF, sizeof(local));

  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();
  hpx_addr_t completed = hpx_lco_future_new(0);
  hpx_gas_memget(&local, remote, sizeof(block), completed);
  hpx_lco_wait(completed);
  hpx_lco_delete(completed, HPX_NULL);
  fprintf(test_log, " Elapsed: %g\n", hpx_time_elapsed_ms(t1));

  for (int i = 0; i < BLOCK_ELEMS; ++i)
    ck_assert_msg(local[i] == block[i],
                  "failed to get element %d correctly, expected %"PRIu64
                  ", got %"PRIu64"\n", i, block[i], local[i]);
}
END_TEST

//****************************************************************************
// Register tests from this file
//****************************************************************************
void add_12_TestMemget(TCase *tc) {
  tcase_add_test(tc, test_libhpx_memget);
}
