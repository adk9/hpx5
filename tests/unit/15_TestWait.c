//****************************************************************************
// @Filename      15_TestWait.c
// @Project       High Performance ParallelX Library (libhpx)
//----------------------------------------------------------------------------
// @Subject       Library Unit Test Harness - Memory Management
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
#include "hpx/hpx.h"
#include "tests.h"
#include <stdlib.h>

#define LCOS_PER_LOCALITY 100000
#define WAITERS 4
#define PARTICIPANTS 4

// ***************************************************************************
// This tests waiting on lcos.  Best run with many cores. Eg., on cutter:
// mpirun -n 2 -map-by node:PE=16 --tag-output ~/repos/hpx-marcin/tests/unit/hpxtest --hpx-cores=16 --hpx-heapsize=$((1024*1024*1024 * 2)) --hpx-transport=mpi
// ***************************************************************************

int t15_set_action(const hpx_addr_t * const future) {
  // printf("Setting %zu on %d\n", *future, HPX_LOCALITY_ID);
  hpx_lco_set(*future, 0, NULL, HPX_NULL, HPX_NULL);
  return HPX_SUCCESS;
}

int t15_wait_action(const hpx_addr_t * const future) {
  // printf("Waiting on %zu on %d\n", lcos[0], HPX_LOCALITY_ID);
  hpx_lco_wait(*future);
  return HPX_SUCCESS;
}

int t15_delete_action(const hpx_addr_t * const lcos) {
  hpx_lco_wait(lcos[2]);
  hpx_lco_delete(lcos[2], HPX_NULL);
  hpx_lco_delete(lcos[0], HPX_NULL);
  hpx_lco_set(lcos[1], 0, NULL, HPX_NULL, HPX_NULL);
  return HPX_SUCCESS;
}

int t15_spawn_action(const hpx_addr_t * const termination_lco) {
  for(size_t i = 0; i < LCOS_PER_LOCALITY; ++i) {
    // test futures
    const hpx_addr_t test_futures[3] = { hpx_lco_future_new(0), *termination_lco, hpx_lco_and_new(WAITERS) };
    hpx_call(HPX_THERE(rand() % HPX_LOCALITIES), t15_set, &test_futures[0], sizeof(hpx_addr_t), HPX_NULL);
    for(size_t j = 0; j < WAITERS; ++j) {
      hpx_call(HPX_THERE(rand() % HPX_LOCALITIES), t15_wait, &test_futures[0], sizeof(hpx_addr_t), test_futures[2]);
    }
    hpx_call(HPX_THERE(rand() % HPX_LOCALITIES), t15_delete, test_futures, sizeof(test_futures), HPX_NULL);

    // test and lco
    const hpx_addr_t test_ands[3] = { hpx_lco_and_new(PARTICIPANTS), *termination_lco, hpx_lco_and_new(WAITERS) };
    for(size_t j = 0; j < PARTICIPANTS; ++j) {
      hpx_call(HPX_THERE(rand() % HPX_LOCALITIES), t15_set, &test_ands[0], sizeof(hpx_addr_t), HPX_NULL);      
    }
    for(size_t j = 0; j < WAITERS; ++j) {
      hpx_call(HPX_THERE(rand() % HPX_LOCALITIES), t15_wait, &test_ands[0], sizeof(hpx_addr_t), test_ands[2]);
    }
    hpx_call(HPX_THERE(rand() % HPX_LOCALITIES), t15_delete, test_ands, sizeof(test_ands), HPX_NULL);
  }
  return HPX_SUCCESS;
}

START_TEST (test_libhpx_lco_wait) {
  fprintf(test_log, "Starting the LCO wait test.\n");
  
  // allocate and start a timer
  const hpx_time_t t1 = hpx_time_now();
  
  const hpx_addr_t termination_lco = hpx_lco_and_new(2 * LCOS_PER_LOCALITY * HPX_LOCALITIES);
  hpx_bcast(t15_spawn, &termination_lco, sizeof(termination_lco), HPX_NULL);
  hpx_lco_wait(termination_lco);
  hpx_lco_delete(termination_lco, HPX_NULL);

  fprintf(test_log, " Elapsed: %g\n", hpx_time_elapsed_ms(t1));
}
END_TEST

//****************************************************************************
// Register tests from this file
//****************************************************************************

void add_15_TestWait(TCase *tc) {
  tcase_add_test(tc, test_libhpx_lco_wait);
}
