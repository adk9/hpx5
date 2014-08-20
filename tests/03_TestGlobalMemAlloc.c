//****************************************************************************
// @Filename      03_TestGloablMemAlloc.c
// @Project       High Performance ParallelX Library (libhpx)
//----------------------------------------------------------------------------
// @Subject       Library Unit Test Harness - Memory Management
// 
// @Compiler      GCC
// @OS            Linux
// @Description   gas.h, lco.h, action.h File Reference
// @Goal          Goal of this testcase is to test the HPX Memory Allocation
//                1. hpx_gas_global_free() -- Free a global allocation.             
//                2. hpx_gas_global_alloc() -- Allocates the distributed 
//                                             global memory
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
// @Date          08/07/2014
// @Author        Jayashree Candadai <jayaajay [at] indiana.edu>
// @Version       1.0
// Commands to Run: make, mpirun hpxtest 
//****************************************************************************

//****************************************************************************
// @Project Includes
//****************************************************************************
#include "hpx/hpx.h"
#include "tests.h"

//****************************************************************************
// TEST: Global Shared Memory (GAS)
//****************************************************************************
static hpx_action_t _globalMemTest	 = 0;

//****************************************************************************
// Action to allocate the global memory and free the memory
//****************************************************************************
static int _action_globalMemTest(void *args) {
  // Global address of the allocated memory
  hpx_addr_t data;
  // Allocate the distributed global memory
  data = hpx_gas_global_alloc(1, 1024 * sizeof(char));
  // Free the global allocation
  hpx_gas_global_free(data, HPX_NULL);
  // Shutdown the HPX runtime
  hpx_shutdown(HPX_SUCCESS);
}

//****************************************************************************
// Test code -- for global memory allocation
//****************************************************************************
START_TEST (test_libhpx_gas_global_alloc)
{
  printf("Starting the GAS global memory allocation test\n");
  // Register the action
  _globalMemTest = HPX_REGISTER_ACTION(_action_globalMemTest);
  // Run the HPX action
  int err = hpx_run(_globalMemTest, NULL, 0);
  ck_assert_msg(err == HPX_SUCCESS, "Could not run the _globalMemTest action");
}
END_TEST

//****************************************************************************
// Register tests from this file
//****************************************************************************

void add_03_TestGlobalMemAlloc(TCase *tc) {
  tcase_add_test(tc, test_libhpx_gas_global_alloc);
}
