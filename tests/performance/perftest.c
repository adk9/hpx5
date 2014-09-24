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
// @Date          10/22/2014
// @Author        Jayashree Candadai <jayaajay [at] indiana.edu>
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
#include <tests.h>
#include <libhpx/debug.h> 
#include <hpx/hpx.h>

//****************************************************************************
//  Globals
//****************************************************************************
hpx_action_t _main = 0;

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
Suite *test_suite(void) 
{
  Suite * s = suite_create("perftest");
  TCase * tc = tcase_create("perftest-core");

  /* install fixtures */
  tcase_add_unchecked_fixture(tc, perftest_core_setup, perftest_core_teardown);

  /* set timeout */
  tcase_set_timeout(tc, 8000);

  add_primesieve(tc);

  suite_add_tcase(s, tc);
  return s;
}

int _main_action(void *args)
{
  Suite *s;
  s = test_suite();
  SRunner * sr = srunner_create(s);
  srunner_add_suite(sr, s);

  //Outputs the result to output.log
  srunner_set_log(sr, "output.log");

  // This sets CK_FORK=no
  srunner_set_fork_status(sr, CK_NOFORK);

  srunner_run_all(sr, CK_VERBOSE);
  srunner_print(sr, CK_VERBOSE);

  int failed = srunner_ntests_failed(sr);
  srunner_free(sr);

  return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

//****************************************************************************
// Registers functions as actions.
//****************************************************************************
void _register_actions(void) {
  _main = HPX_REGISTER_ACTION(_main_action);
}

//****************************************************************************
// Initialize the check and run as a HPX application
//****************************************************************************
int main(int argc, char * argv[]) {
  //dbg_wait();
  
  hpx_config_t cfg = HPX_CONFIG_DEFAULTS;

  // parse the command line
  int opt = 0;
  while ((opt = getopt(argc, argv, "c:t:d:T:Dh")) != -1) {
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
     case 'T':
      cfg.transport = atoi(optarg);
      assert(0 <= cfg.transport && cfg.transport < HPX_TRANSPORT_MAX);
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
