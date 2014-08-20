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
//                1. hpx_init() initializes the scheduler, network and
//                   locality.
//                2. hpx_shutdown() called by the application to terminate
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
#include "common.h"

FILE * perf_log;

static hpx_action_t _main = 0;
//****************************************************************************
// UnitTest_Main_Action: Action called to terminate the scheduler and network
// during hpxtest_core_teardown using hpx_shutdown.
//****************************************************************************

int unittest_main_action(const main_args_t *args)
{
  printf("Shutting down the HPX action in hpxtest_core_teardown function\n");
  // Shutdown the HPX runtime. This causes the hpx_run() in the main native 
  // thread to return the code. The returned thread is executing the original
  // native thread, and all supplementary scheduler threads and network will 
  // have been shutdown, and any library resources will have been cleaned up.
  hpx_shutdown(0);
}


//****************************************************************************
// TEST SUITE FIXTURE: Library initialization
//****************************************************************************
void hpxtest_core_setup(void) {
  /* initialize libhpx */
  printf("Initializing the Library test suite in hpxtest_core_setup \n");
  // Initializes the HPX runtime. This must be called before other HPX functions.
  // Parameters: config HPX runtime configuration; may be HPX_NULL
  int err = hpx_init(NULL);
  // hpx_init returns HPX_SUCCESS on success.
  ck_assert_msg(err == HPX_SUCCESS, "Could not initialize libhpx");

  // Register the main action (user-level action with the runtime
  _main = HPX_REGISTER_ACTION(unittest_main_action);

  /* open a performance log file */
  perf_log = fopen("perf.log", "w+");
  ck_assert_msg(perf_log != NULL, "Could not open performance log");
}

//****************************************************************************
//  TEST SUITE FIXTURE: library cleanup
//****************************************************************************

void hpxtest_core_teardown(void) {
  fclose(perf_log);
  // Start the HPX Runtime. This finalizes action registration, starts up any 
  // scheduler and native threads that need to run, and transfers all control
  // into the HPX scheduler, beginning execution in entry.
  hpx_run(_main, NULL, 0);
}


