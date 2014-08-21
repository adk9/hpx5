//****************************************************************************
// @Filename      01_TestRuntime.c
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
// @Version       1.0
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

FILE * perf_log;

//****************************************************************************
// TEST SUITE FIXTURE: Library initialization
//****************************************************************************
void hpxtest_core_setup(void) {
  /* open a performance log file */
  perf_log = fopen("perf.log", "w+");
  ck_assert_msg(perf_log != NULL, "Could not open performance log");
}

//****************************************************************************
//  TEST SUITE FIXTURE: library cleanup
//****************************************************************************

void hpxtest_core_teardown(void) {
  fclose(perf_log);
  hpx_shutdown(0);
}


