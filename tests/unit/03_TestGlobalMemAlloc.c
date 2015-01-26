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
// @Version       0.1
// Commands to Run: make, mpirun hpxtest 
//****************************************************************************

//****************************************************************************
// @Project Includes
//****************************************************************************
#include "hpx/hpx.h"
#include "tests.h"
#include "domain.h"

typedef struct {
  int nDoms;
  int maxCycles;
  int cores;
} main_args_t;

typedef struct Domain {
  hpx_addr_t complete;
  hpx_addr_t newdt;
  int nDoms;
  int rank;
  int maxcycles;
  int cycle;
} Domain;

/// Initialize a domain.
int
t03_initDomain_action(const InitArgs *args)
{
  // Get the address this parcel was sent to, and map it to a local address---if
  // this fails then the message arrived at the wrong place due to AGAS
  // movement, so resend the parcel.
  hpx_addr_t local = hpx_thread_current_target();
  Domain *ld = NULL;
  if (!hpx_gas_try_pin(local, (void**)&ld))
    return HPX_RESEND;

  // Update the domain with the argument data.
  ld->rank = args->index;
  ld->maxcycles = args->maxcycles;
  ld->nDoms = args->nDoms;

  // make sure to unpin the domain, so that AGAS can move it if it wants to
  hpx_gas_unpin(local);

  //fprintf(test_log, "Initialized domain %u\n", args->index);

  // return success---this triggers whatever continuation was set by the parcel
  // sender
  return HPX_SUCCESS;
}

//****************************************************************************
// Test code -- for global memory allocation
//****************************************************************************
START_TEST (test_libhpx_gas_global_alloc)
{
  // allocate the default argument structure on the stack
  
  main_args_t args = {
    .nDoms = 8,
    .maxCycles = 1,
    .cores = 8
  };
  
  fprintf(test_log, "Starting the GAS global memory allocation test\n");
  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

 // output the arguments we're running with
  fprintf(test_log, "Number of domains: %d maxCycles: %d cores: %d\n",
         args.nDoms, args.maxCycles, args.cores);
  fflush(test_log);
  
  // Allocate the domain array
  hpx_addr_t domain = hpx_gas_global_alloc(args.nDoms, sizeof(Domain));
  
  // Allocate an and gate that we can wait on to detect that all of the domains
  // have completed initialization.
  hpx_addr_t done = hpx_lco_and_new(args.nDoms);

  // Send the initDomain action to all of the domains, in parallel.
  for (int i = 0, e = args.nDoms; i < e; ++i) {

    // hpx_call() will copy this
    InitArgs init = {
      .index = i,
      .nDoms = args.nDoms,
      .maxcycles = args.maxCycles,
      .cores = args.cores
    };

    // compute the offset for this domain
    hpx_addr_t block = hpx_addr_add(domain, sizeof(Domain) * i, sizeof(Domain));

    // and send the initDomain action, with the done LCO as the continuation
    hpx_call(block, t03_initDomain, done, &init, sizeof(init));
  }

  // wait for initialization
  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);
  
  // and free the domain
  hpx_gas_free(domain, HPX_NULL);

  fprintf(test_log, " Elapsed: %g\n", hpx_time_elapsed_ms(t1));
}
END_TEST

START_TEST(test_libhpx_gas_global_alloc_block)
{
  hpx_addr_t data = hpx_gas_global_alloc(1, 1024 * sizeof(char));
  hpx_gas_free(data, HPX_NULL);
}
END_TEST

#define N 2000

typedef struct inputDomain {
 int rank;
} inputDomain;

int t03_printHello_action(int *value)
{
  hpx_addr_t local = hpx_thread_current_target();
  inputDomain *ld = NULL;
  if (!hpx_gas_try_pin(local, (void**)&ld))
    return HPX_RESEND;

  ld->rank = *value;
 
  hpx_gas_unpin(local);
  return HPX_SUCCESS;
}

START_TEST(test_libhpx_gas_global_alloc_big_blocks) 
{
  int size = HPX_THREADS * HPX_LOCALITIES * N * N;
  hpx_addr_t domain = hpx_gas_global_alloc(size, sizeof(domain));
  hpx_addr_t done = hpx_lco_and_new(size);

  for (int i = 0; i < size; i++) {
    hpx_addr_t block = hpx_addr_add(domain, sizeof(inputDomain)*i, 
                                    sizeof(inputDomain));
    hpx_call(block, t03_printHello, done, &i, sizeof(int));
  }

  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);

  hpx_gas_free(domain, HPX_NULL);
}
END_TEST

//****************************************************************************
// Register tests from this file
//****************************************************************************

void add_03_TestGlobalMemAlloc(TCase *tc) {
  tcase_add_test(tc, test_libhpx_gas_global_alloc);
  tcase_add_test(tc, test_libhpx_gas_global_alloc_block);
  //tcase_add_test(tc, test_libhpx_gas_global_alloc_big_blocks);
}
