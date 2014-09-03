//****************************************************************************
// @Filename      00_hpxtest.c
// @Project       High Performance ParallelX Library (libhpx)
//----------------------------------------------------------------------------
// @Subject       Library Unit Test Harness
// 
// @Compiler      GCC
// @OS            Linux
// @Description   Main function. 
// @Goal          Goal of this testcase is to initialize check as HPX 
//                application   
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
// @Author        Patrick K. Bohan <pbohan [at] indiana.edu>
//                Jayashree Candadai <jayaajay [at] indiana.edu>
// @Version       0.1
// Commands to Run: make, mpirun hpxtest 
//****************************************************************************

//****************************************************************************
// @Project Includes
//****************************************************************************
#include <stdlib.h>                             
#include <unistd.h>
#include <assert.h>
#include <limits.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <check.h>

#include <libhpx/debug.h> 
#include <hpx/hpx.h>

#include "tests.h"

//****************************************************************************
//  Globals
//****************************************************************************
hpx_action_t _main = 0;

hpx_action_t t02_init_sources;
hpx_action_t t03_initDomain;
hpx_action_t t04_send;
hpx_action_t t04_sendData;
hpx_action_t t04_recv;
hpx_action_t t04_getContValue;
hpx_action_t t05_initData;
hpx_action_t t05_worker;
hpx_action_t t05_assignID;
hpx_action_t t05_cont_thread;
hpx_action_t t05_thread_cont_cleanup;
hpx_action_t t05_thread_current_cont_target;
hpx_action_t t05_thread_yield_producer;
hpx_action_t t05_thread_yield_consumer;
hpx_action_t t06_set_value;
hpx_action_t t06_get_value;
hpx_action_t t06_get_future_value;
 
//****************************************************************************
// Options
//****************************************************************************
static void usage(FILE *f) {
  fprintf(f, "Usage: CHECK [options] ROUNDS \n"
          "\t-c, cores\n"
          "\t-t, scheduler threads\n"
          "\t-D, all localities wait for debugger\n"
          "\t-d, wait for debugger at specific locality\n"
          "\t-h, show help\n");
}

//****************************************************************************
// Main action to run check as HPX Application
//****************************************************************************
static int _main_action(void *args)
{
  Suite * s = suite_create("hpxtest");
  TCase * tc = tcase_create("hpxtest-core");
  
  /* install fixtures */
  tcase_add_unchecked_fixture(tc, hpxtest_core_setup, hpxtest_core_teardown);

  /* set timeout */
  tcase_set_timeout(tc, 8000);

  add_02_TestMemAlloc(tc);
  add_03_TestGlobalMemAlloc(tc);
  add_04_TestParcel(tc);
  add_05_TestThreads(tc);
  add_06_TestFutures(tc);  

  suite_add_tcase(s, tc);

  SRunner * sr = srunner_create(s);
  srunner_add_suite(sr, s);

  //Outputs the result to test.log
  srunner_set_log(sr, "test.log");

  // This sets CK_FORK=no
  srunner_set_fork_status(sr, CK_NOFORK);

  srunner_run_all(sr, CK_NORMAL);

  int failed = srunner_ntests_failed(sr);
  srunner_free(sr);

  return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

//****************************************************************************
// Registers functions as actions.
//****************************************************************************
void _register_actions(void) {
  _main = HPX_REGISTER_ACTION(_main_action);

  // 02_TestMemAlloc.c
  t02_init_sources = HPX_REGISTER_ACTION(t02_init_sources_action);

  // 03_TestGlobalMemAlloc.c
  t03_initDomain = HPX_REGISTER_ACTION(t03_initDomain_action);

  //04_TestParcel.c
  t04_send = HPX_REGISTER_ACTION(t04_send_action);
  t04_sendData = HPX_REGISTER_ACTION(t04_sendData_action);
  t04_recv = HPX_REGISTER_ACTION(t04_recv_action);
  t04_getContValue = HPX_REGISTER_ACTION(t04_getContValue_action);

  //05_TestThreads.c
  t05_initData = HPX_REGISTER_ACTION(t05_initData_action);
  t05_worker   = HPX_REGISTER_ACTION(t05_worker_action);
  t05_assignID = HPX_REGISTER_ACTION(t05_assignID_action);
  t05_cont_thread = HPX_REGISTER_ACTION(t05_set_cont_action);
  t05_thread_cont_cleanup = HPX_REGISTER_ACTION(t05_thread_cont_cleanup_action);
  t05_thread_current_cont_target = HPX_REGISTER_ACTION(t05_thread_current_cont_target_action);
  t05_thread_yield_producer = HPX_REGISTER_ACTION(t05_thread_yield_producer_action);
  t05_thread_yield_consumer = HPX_REGISTER_ACTION(t05_thread_yield_consumer_action);

  //06_TestFutures.c
  t06_set_value = HPX_REGISTER_ACTION(t06_set_value_action);
  t06_get_value = HPX_REGISTER_ACTION(t06_get_value_action);
  t06_get_future_value = HPX_REGISTER_ACTION(t06_get_future_value_action);
}

//****************************************************************************
// Initialize the check and run as a HPX application
//****************************************************************************
int main(int argc, char * argv[]) {
  //dbg_wait();
  
  hpx_config_t cfg = HPX_CONFIG_DEFAULTS;

  // parse the command line
  int opt = 0;
  while ((opt = getopt(argc, argv, "c:t:d:Dh")) != -1) {
    switch (opt) {
     case 'c':
      cfg.cores = atoi(optarg);
      break;
     case 't':
      cfg.threads = atoi(optarg);
      break;
     case 'D':
      cfg.wait = HPX_WAIT;
      cfg.wait_at = HPX_LOCALITY_ALL;
      break;
     case 'd':
      cfg.wait = HPX_WAIT;
      cfg.wait_at = atoi(optarg);
      break;
     case 'h':
      usage(stdout);
      return 0;
     case '?':
     default:
      usage(stderr);
      return -1;
    }
  }

  // Initialize HPX
  hpx_init(&cfg);

  // Register all the actions
  _register_actions();

  // Run HPX 
  return hpx_run(_main, NULL, 0);
}
