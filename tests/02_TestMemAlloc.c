//****************************************************************************
// @Filename      02_TestMemAlloc.c
// @Project       High Performance ParallelX Library (libhpx)
//----------------------------------------------------------------------------
// @Subject       Library Unit Test Harness - Memory Management
// 
// @Compiler      GCC
// @OS            Linux
// @Description   gas.h, lco.h, action.h File Reference
// @Goal          Goal of this testcase is to test the HPX Memory Allocation
//                1. hpx_gas_alloc() -- Allocates the global memory. 
//                2. hpx_gas_global_free() -- Free a global allocation.
//                3. hpx_gas_try_pin() -- Performs address translation.
//                4. hpx_gas_unpin() -- Allows an address to be remapped.   
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
static hpx_action_t _localMemTest	 = 0;

/// @brief Initialize sources action
static hpx_action_t _init_sources 	 = 0;
static int _init_sources_action(void); 

//****************************************************************************
// Source action to populate the data
//****************************************************************************
static int _init_sources_action(void) {
  printf("Populating the data\n");
  // Get the global address.
  hpx_addr_t curr = hpx_thread_current_target();
  // The pinned local address 
  int *sources_p = NULL; 

  // Performs address translation. This will try to perform a global-to-local
  // translation on the global addr, and set local to the local address if it
  // it is successful.
  bool pinned = hpx_gas_try_pin(curr, (void **)&sources_p);
  ck_assert_msg(pinned == true, "Could not perform the address translation");

  for (int i = 0; i < 10; i++){
     sources_p[i] = i;
     printf("Sources_p[i] = '%d'\n", sources_p[i]);
  }

  // Allows address to be remapped. curr -- the address of global memory to 
  // unpin
  hpx_gas_unpin(curr);
  hpx_thread_exit(HPX_SUCCESS);
}

//****************************************************************************
// Action to allocate the GAS local memory and call the thread to populate the
// data
//****************************************************************************
static int _action_localMemTest(void *args) {
  // The global address of the allocated local global memory
  hpx_addr_t local;
  static double *data_p = NULL;
  // the number of bytes to allocate
  int ndata = 10;

  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

  // Allocate the local global memory to hold the data of 10 bytes. 
  // This is a non-collective, local call to allocate memory in the global 
  // address space that can be moved. This allocates one block with 10
  // bytes of memory
  local = hpx_gas_alloc(1, ndata * sizeof(double));

  // Populate the test data -- Allocate an and gate that we can wait on to
  // detect that all of the data have been populated. This creates a future.
  // Futures are builtin LCOs that represent values returned from asynchronous 
  // computation. Futures are always allocated in the global address space, 
  // because their addresses are used as the targets of parcels.
  hpx_addr_t next =  hpx_lco_future_new(sizeof(double));
  // and send the init_sources action, with the done LCO as the continuation
  hpx_call(local, _init_sources, NULL, 0, next);

  // wait for initialization. The LCO blocks the caller until an LCO set 
  // operation triggers the LCO. 
  int err = hpx_lco_wait(next);
  ck_assert_msg(err == HPX_SUCCESS, "hpx_lco_wait propagated error");
  
  // Deletes an LCO - next -the address of the lco to delete
  hpx_lco_delete(next, HPX_NULL);
  
  // Cleanup - Free the global allocation of local global memory.
  hpx_gas_global_free(local, HPX_NULL);
  
  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));

  // Shutdown the HPX runtime 
  hpx_shutdown(HPX_SUCCESS);
}

//****************************************************************************
// Test code -- for GAS local memory allocation
//****************************************************************************
START_TEST (test_libhpx_gas_alloc)
{
  printf("Starting the GAS local memory allocation test\n");
  // Register the actions
  _localMemTest = HPX_REGISTER_ACTION(_action_localMemTest);
  _init_sources = HPX_REGISTER_ACTION(_init_sources_action);
  // Run the HPX action
  int err = hpx_run(_localMemTest, NULL, 0);
  ck_assert_msg(err == HPX_SUCCESS, "Could not run the _localMemTest action");
} 
END_TEST

//****************************************************************************
// Register tests from this file
//****************************************************************************

void add_02_TestMemAlloc(TCase *tc) {
  tcase_add_test(tc, test_libhpx_gas_alloc);
}
