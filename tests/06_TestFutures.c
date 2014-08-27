//****************************************************************************
// @Filename      06_TestFuture.c
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

int t06_initDomain_action(const InitArgs *args)
{
  hpx_addr_t local = hpx_thread_current_target();
  Domain *ld = NULL;
  if (!hpx_gas_try_pin(local, (void**)&ld))
    return HPX_RESEND;

  ld->rank = args->index;
  ld->maxcycles = args->maxcycles;
  ld->nDoms = args->nDoms;
  ld->complete = args->complete;
  ld->cycle = 0;

  // record the newdt alltoall
  ld->newdt = args->newdt;

  hpx_gas_unpin(local);

  fflush(stdout);
  return HPX_SUCCESS;
}

int t06_advanceDomain_action(const unsigned long *epoch)
{
  hpx_addr_t local = hpx_thread_current_target();
  Domain *domain = NULL;
  if (!hpx_gas_try_pin(local, (void**)&domain))
    return HPX_RESEND;

  if (domain->maxcycles <= domain->cycle) {
    hpx_lco_set(domain->complete, 0, NULL, HPX_NULL, HPX_NULL);
    hpx_gas_unpin(local);
    return HPX_SUCCESS;
  }

  // Compute my gnewdt, and then start the future
  int gnewdt[domain->nDoms];
  for (int k=0; k < domain->nDoms; k++) {
    gnewdt[k] = (k+1) + domain->rank * domain->nDoms;
  }

  //hpx_lco_future_setat(domain->newdt, domain->rank, domain->nDoms * sizeof(int), gnewdt, HPX_NULL, HPX_NULL);

  // Get the gathered value, and print the debugging string.
  int newdt[domain->nDoms];
  //hpx_lco_future_getat(domain->newdt, domain->rank, sizeof(newdt), &newdt);

  ++domain->cycle;
  const unsigned long next = *epoch + 1;
  return hpx_call(local, t06_advanceDomain, &next, sizeof(next), HPX_NULL);
}


//****************************************************************************
// Test code -- for Future LCO test
//****************************************************************************
START_TEST (test_libhpx_future_lco)
{
  printf("Starting the Future LCO test\n");
  // allocate the default argument structure on the stack
  main_args_t args = {
    .nDoms = 8,
    .maxCycles = 1,
    .cores = 8
  };

  printf("Number of domains: %d maxCycles: %d cores: %d\n",
         args.nDoms, args.maxCycles, args.cores);
  fflush(stdout);

  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t domain = hpx_gas_alloc(args.nDoms, sizeof(Domain));
  hpx_addr_t done = hpx_lco_future_new(args.nDoms);
  hpx_addr_t complete = hpx_lco_future_new(args.nDoms);


  //TODO: Call the futures new function here
  //hpx_addr_t newdt = hpx_lco_future_new_all(args->nDoms, 
  //                                          args->nDoms * sizeof(int));

  for (int i = 0, e = args.nDoms; i < e; ++i) {
    InitArgs init = {
      .index = i,
      .nDoms = args.nDoms,
      .maxcycles = args.maxCycles,
      .cores = args.cores,
      .complete = complete,
      //.newdt = newdt
    };
    hpx_addr_t block = hpx_addr_add(domain, sizeof(Domain) * i);
    hpx_call(block, t06_initDomain, &init, sizeof(init), done);
  }

  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);

  const unsigned long epoch = 0;
  for (int i = 0, e = args.nDoms; i < e; ++i) {
    hpx_addr_t block = hpx_addr_add(domain, sizeof(Domain) * i);
    hpx_call(block, t06_advanceDomain, &epoch, sizeof(epoch), HPX_NULL);
  }

  hpx_lco_wait(complete);
  hpx_lco_delete(complete, HPX_NULL);

  hpx_gas_free(domain, HPX_NULL);
  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
} 
END_TEST

//****************************************************************************
// Register tests from this file
//****************************************************************************

void add_06_TestFutures(TCase *tc) {
  tcase_add_test(tc, test_libhpx_future_lco);
}
