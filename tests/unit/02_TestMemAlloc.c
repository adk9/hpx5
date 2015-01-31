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
// @Version       0.1
// Commands to Run: make, mpirun hpxtest 
//****************************************************************************

//****************************************************************************
// @Project Includes
//****************************************************************************
#include "hpx/hpx.h"
#include "tests.h"

//****************************************************************************
// Source action to populate the data
//****************************************************************************
int t02_init_sources_action(void* args) {
  fprintf(test_log, "Populating the data\n");
  // Get the address this parcel was sent to, and map it to a local address---if
  // this fails then the message arrived at the wrong place due to AGAS
  // movement, so resend the parcel.
  hpx_addr_t local = hpx_thread_current_target();

  // The pinned local address 
  int *sources_p = NULL;

  // Performs address translation. This will try to perform a global-to-local
  // translation on the global addr, and set local to the local address if it
  // it is successful.
  if (!hpx_gas_try_pin(local, (void **)&sources_p))
    return HPX_RESEND;

  for (int i=0; i<10; i++){
    sources_p[i] = i;
    //fprintf(test_log, "Sources_p[i] = '%d'\n", sources_p[i]);
  }

  // make sure to unpin the domain, so that AGAS can move it if it wants to
  hpx_gas_unpin(local);

  return HPX_SUCCESS;
}

//****************************************************************************
// Test code -- for GAS local memory allocation
//****************************************************************************
START_TEST (test_libhpx_gas_alloc)
{
  fprintf(test_log, "Starting the GAS local memory allocation test\n");
  hpx_addr_t local;
  // the number of bytes to allocate
  int ndata = 10;

  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

  // Allocate the local global memory to hold the data of 10 bytes. 
  // This is a non-collective, local call to allocate memory in the global 
  // address space that can be moved. This allocates one block with 10
  // bytes of memory
  local = hpx_gas_alloc(ndata * sizeof(double));

  // Populate the test data -- Allocate an and gate that we can wait on to
  // detect that all of the data have been populated. This creates a future.
  // Futures are builtin LCOs that represent values returned from asynchronous 
  // computation. Futures are always allocated in the global address space, 
  // because their addresses are used as the targets of parcels.
  hpx_addr_t done = hpx_lco_future_new(sizeof(double));
  
  // and send the init_sources action, with the done LCO as the continuation
  hpx_call(local, t02_init_sources, done, NULL, 0);

  // wait for initialization. The LCO blocks the caller until an LCO set 
  // operation triggers the LCO. 
  int err = hpx_lco_wait(done);
  ck_assert_msg(err == HPX_SUCCESS, "hpx_lco_wait propagated error");

  // Deletes an LCO - done -the address of the lco to delete
  hpx_lco_delete(done, HPX_NULL);

  // Cleanup - Free the global allocation of local global memory.
  hpx_gas_free(local, HPX_NULL);  

  fprintf(test_log, " Elapsed: %g\n", hpx_time_elapsed_ms(t1));
} 
END_TEST

//****************************************************************************
// Register tests from this file
//****************************************************************************

void add_02_TestMemAlloc(TCase *tc) {
  tcase_add_test(tc, test_libhpx_gas_alloc);
}
