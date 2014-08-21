//****************************************************************************
// @Filename      04_TestMemMove.c
// @Project       High Performance ParallelX Library (libhpx)
//----------------------------------------------------------------------------
// @Subject       Library Unit Test Harness - Memory Management
// 
// @Compiler      GCC
// @OS            Linux
// @Description   gas.h, lco.h, action.h File Reference
// @Goal          Goal of this testcase is to test the HPX Memory Allocation
//                1. hpx_gas_move() -- Change the locality affinity of a 
//                                     global distributed memory address.            
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
// @Date          08/20/2014
// @Author        Jayashree Candadai <jayaajay [at] indiana.edu>
// @Version       1.0
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

//****************************************************************************
// TEST: Global Shared Memory (GAS)
//****************************************************************************

int t04_get_rank_action(void *args) {
  int rank = HPX_LOCALITY_ID;
  HPX_THREAD_CONTINUE(rank);
}

//****************************************************************************
// A thread on the root locality allocates two futures with a cyclic 
// distribution, one on the root locality and the other on a remote locality.
// It invokes a "get rank" action on the remote future, initiates a 
// synchronous move of the remote future address to the root locality, and
// re-executes the "get-rank" action on the address.
//****************************************************************************
int t04_root_action(void *args) {
  printf("root locality: %d, thread: %d.\n", HPX_LOCALITY_ID, HPX_THREAD_ID);
  hpx_addr_t base = hpx_lco_future_array_new(2, sizeof(int), 1);
  hpx_addr_t other = hpx_lco_future_array_at(base, 1);

  int r = 0;
  hpx_call_sync(other, t04_get_rank, NULL, 0, &r, sizeof(r));
  printf("target locality's ID (before move): %d\n", r);

  if (r == HPX_LOCALITY_ID) {
    printf("AGAS test: failed.\n");
    hpx_shutdown(0);
  }

  hpx_addr_t done = hpx_lco_future_new(0);
  // move address to our locality.
  printf("initiating AGAS move from (%lu,%u,%u) to (%lu,%u,%u).\n",
         other.offset, other.base_id, other.block_bytes,
         HPX_HERE.offset, HPX_HERE.base_id, HPX_HERE.block_bytes);

  // other is the source address of the move, HPX_HERE is the destination -  
  // address pointing to the target locality to move the source address other
  // to and done is the LCO object to check to wait for the completion of move.
  hpx_gas_move(other, HPX_HERE, done);

  if (hpx_lco_wait(done) != HPX_SUCCESS)
    printf("error in hpx_move().\n");

  hpx_lco_delete(done, HPX_NULL);

  hpx_call_sync(other, t04_get_rank, NULL, 0, &r, sizeof(r));
  printf("target locality's rank (after move): %d\n", r);

  printf("AGAS test: %s.\n", ((r == hpx_get_my_rank()) ? "passed" : "failed"));
  return HPX_SUCCESS;
}

//****************************************************************************
// Test code -- for GAS Move: Change the locality affinity of a global 
// distributed memory address. This operation is only valid in the AGAS GAS
// mode. For PGAS it is effective a noop
//****************************************************************************
START_TEST (test_libhpx_gas_move)
{
  hpx_addr_t local;
  hpx_config_t cfg = {
    .cores       = 4,
    .threads     = 2,
    .stack_bytes = 0,
    .gas         = HPX_GAS_AGAS
  };

  printf("Starting the change the locality affinity of a GAS test\n");
  int ranks = HPX_LOCALITIES;
  if (ranks < 2) {
    fprintf(stderr, "A minimum of 2 localities are required to run this test.");
  }

  local = hpx_gas_alloc(1, sizeof(double));
  
  hpx_addr_t completed =  hpx_lco_future_new(sizeof(double));

  hpx_call(local, t04_root,  NULL, 0, completed);

  int err = hpx_lco_wait(completed);
  ck_assert_msg(err == HPX_SUCCESS, "hpx_lco_wait propagated error");

  // Deletes an LCO - next -the address of the lco to delete
  hpx_lco_delete(completed, HPX_NULL);

  // Cleanup - Free the global allocation of local global memory.
  hpx_gas_global_free(local, HPX_NULL);
 
}
END_TEST

//****************************************************************************
// Register tests from this file
//****************************************************************************

void add_04_TestMemMove(TCase *tc) {
  tcase_add_test(tc, test_libhpx_gas_move);
}
