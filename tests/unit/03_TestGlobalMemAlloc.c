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

static int _initDomain_handler(int rank, int max, int n) {
  Domain *ld = hpx_thread_current_local_target();
  ld->rank = rank;
  ld->maxcycles = max;
  ld->nDoms = n;
  return HPX_SUCCESS;
}

HPX_ACTION_DEF(PINNED, _initDomain_handler, _initDomain, HPX_INT, HPX_INT, HPX_INT);

//****************************************************************************
// Test code -- for global memory allocation
//****************************************************************************
START_TEST (test_libhpx_gas_global_alloc)
{
  // allocate the default argument structure on the stack

  int nDoms = 8;
  int maxCycles = 1;

  fprintf(test_log, "Starting the GAS global memory allocation test\n");
  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

 // output the arguments we're running with
  fprintf(test_log, "Number of domains: %d maxCycles: %d cores: %d\n",
          nDoms, maxCycles, 8);
  fflush(test_log);

  // Allocate the domain array
  hpx_addr_t domain = hpx_gas_global_alloc(nDoms, sizeof(Domain));

  // Allocate an and gate that we can wait on to detect that all of the domains
  // have completed initialization.
  hpx_addr_t done = hpx_lco_and_new(nDoms);

  // Send the initDomain action to all of the domains, in parallel.
  for (int i = 0, e = nDoms; i < e; ++i) {
    // compute the offset for this domain and send the initDomain action, with
    // the done LCO as the continuation
    hpx_addr_t block = hpx_addr_add(domain, sizeof(Domain) * i, sizeof(Domain));
    hpx_call(block, _initDomain, done, &i, &maxCycles, &nDoms);
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

//****************************************************************************
// Register tests from this file
//****************************************************************************

void add_03_TestGlobalMemAlloc(TCase *tc) {
  tcase_add_test(tc, test_libhpx_gas_global_alloc);
  tcase_add_test(tc, test_libhpx_gas_global_alloc_block);
}
