//****************************************************************************
// @Filename      perfinit.c
// @Project       High Performance ParallelX Library (libhpx)
//----------------------------------------------------------------------------
// @Subject       Library Unit Test Harness - Initialization and Cleanup 
//                Functions
// 
// @Compiler      GCC
// @OS            Linux
// @Description   runtime.h File Reference
// @Goal          Goal of this testcase is to test the HPX system interface
//                1. hpx_shutdown() called by the application to terminate
//                   the scheduler and network.
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
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>

//****************************************************************************
// TEST SUITE FIXTURE: Library initialization
//****************************************************************************
void perftest_core_setup(void) {
  /* open a performance log file */
  printf("Starting the HPX performance test framework\n");
}

//****************************************************************************
//  TEST SUITE FIXTURE: library cleanup
//****************************************************************************

void perftest_core_teardown(void) {
  printf("Shutting down HPX performance test framework in perftest_core_teardown\n");
  printf("Check output.log for output\n\n");
  hpx_shutdown(0);
}


