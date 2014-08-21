
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Library Unit Test Harness
  00_hpxtest.c

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).

  Authors:
    Patrick K. Bohan <pbohan [at] indiana.edu>
 ====================================================================
*/


#include <stdlib.h>                             /* getenv */
#include <unistd.h>
#include <assert.h>
#include <limits.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include <libhpx/debug.h> 
#include <hpx/hpx.h>

#include "tests.h"
#include "common.h"

/*
  Globals
 */
hpx_action_t t02_init_sources;
hpx_action_t t03_initDomain;
hpx_action_t t04_root;
hpx_action_t t04_get_rank;


/*
 --------------------------------------------------------------------
  Main
 --------------------------------------------------------------------
*/
static void usage(FILE *f) {
  fprintf(f, "Usage: CHECK [options] ROUNDS \n"
          "\t-c, cores\n"
          "\t-t, scheduler threads\n"
          "\t-D, all localities wait for debugger\n"
          "\t-d, wait for debugger at specific locality\n"
          "\t-h, show help\n");
}

static int _main_action(void *args)
{
  Suite * s = suite_create("hpxtest");
  TCase * tc = tcase_create("hpxtest-core");
  char * long_tests = NULL;
  char * hardcore_tests = NULL;
  char * perf_tests = NULL;

  /* figure out if we need to run long-running tests */
  long_tests = getenv("HPXTEST_EXTENDED");

  /* see if we're in HARDCORE mode (heh) */
  hardcore_tests = getenv("HPXTEST_HARDCORE");

  /* see if we're supposed to run performance tests */
  perf_tests = getenv("HPXTEST_PERF");

  /* install fixtures */
  tcase_add_unchecked_fixture(tc, hpxtest_core_setup, hpxtest_core_teardown);

  /* set timeout */
  tcase_set_timeout(tc, 8000);

  add_02_TestMemAlloc(tc);
  //add_03_TestGlobalMemAlloc(tc);
  //add_04_TestMemMove(tc);
  add_05_TestParcel(tc);

  suite_add_tcase(s, tc);

  SRunner * sr = srunner_create(s);
  srunner_run_all(sr, CK_NORMAL);

  int failed = srunner_ntests_failed(sr);
  srunner_free(sr);

  return (failed == 0) ? 0 : -1;
}

/**
 * Registers functions as actions.
**/
void _register_actions(void) {
  _main = HPX_REGISTER_ACTION(_main_action);

  // 02_TestMemAlloc.c
  t02_init_sources = HPX_REGISTER_ACTION(t02_init_sources_action);

  // 03_TestGlobalMemAlloc.c
  t03_initDomain = HPX_REGISTER_ACTION(t03_initDomain_action);

  // 04_TestMemMove.c
  t04_root     = HPX_REGISTER_ACTION(t04_root_action);
  t04_get_rank = HPX_REGISTER_ACTION(t04_get_rank_action);

  //_init_sources = HPX_REGISTER_ACTION(_init_sources_action);
}

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

  printf("Initializing the Library test suite in main function \n");
  // Initialize HPX
  hpx_init(&cfg);

  // Register all the actions
  _register_actions();

  // Run HPX 
  return hpx_run(_main, NULL, 0);
}
