
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
#include "tests.h"
#include "hpx/hpx.h"

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

typedef struct {
  int nDoms;
  int maxCycles;
  int cores;
} main_args_t;

static void
_usage(FILE *f, int error) {
  fprintf(f, "Usage: ./example [options]\n"
          "\t-c, cores\n"
          "\t-t, scheduler threads\n"
          "\t-s, stack size in bytes\n"
          "\t-D, all localities wait for debugger\n"
          "\t-d, wait for debugger at specific locality\n"
          "\t-n, number of domains\n"
          "\t-i, maxcycles\n"
          "\t-h, show help\n");
  fflush(f);
  exit(error);
}

static hpx_action_t _main = 0;

int _main_action(const main_args_t *args)
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
  add_03_TestGlobalMemAlloc(tc);
  //add_04_TestMemMove(tc);

  suite_add_tcase(s, tc);

  SRunner * sr = srunner_create(s);
  srunner_run_all(sr, CK_NORMAL);

  int failed = srunner_ntests_failed(sr);
  srunner_free(sr);

  return (failed == 0) ? 0 : -1;
}

int main(int argc, char * argv[]) {
   // allocate the default argument structure on the stack
  main_args_t args = {
    .nDoms = 8,
    .maxCycles = 1,
    .cores = 8
  };

  // allocate the default HPX configuration on the stack
  hpx_config_t cfg = {
    .cores = args.cores,
    .threads = args.cores,
    .stack_bytes = 0,
    .gas = HPX_GAS_PGAS
  };

  // parse the command line
  int opt = 0;
  while ((opt = getopt(argc, argv, "c:t:s:d:D:n:i:h")) != -1) {
    switch (opt) {
     case 'c':
      args.cores = cfg.cores = atoi(optarg);
      break;
     case 't':
      cfg.threads = atoi(optarg);
      break;
     case 's':
      cfg.stack_bytes = atoi(optarg);
      break;
     case 'D':
      cfg.wait = HPX_WAIT;
      cfg.wait_at = HPX_LOCALITY_ALL;
      break;
     case 'd':
      cfg.wait = HPX_WAIT;
      cfg.wait_at = atoi(optarg);
      break;
     case 'n':
      args.nDoms = atoi(optarg);
      break;
     case 'i':
      args.maxCycles = atoi(optarg);
      break;
     case 'h':
      _usage(stdout, 0);
     case '?':
     default:
      _usage(stderr, -1);
    }
  }

  printf("Initializing the Library test suite in main function \n");
  // Initialize HPX
  int err = hpx_init(&cfg);
  if (err)
    return err;

  // Register the main action (user-level action with the runtime)
  _main = HPX_REGISTER_ACTION(_main_action);
  // 02_TestMemAlloc.c
  t02_init_sources = HPX_REGISTER_ACTION(t02_init_sources_action);
  // 03_TestGlobalMemAlloc.c
  t03_initDomain = HPX_REGISTER_ACTION(t03_initDomain_action);
  // 04_TestMemMove.c
  t04_root     = HPX_REGISTER_ACTION(t04_root_action);
  t04_get_rank = HPX_REGISTER_ACTION(t04_get_rank_action);

  // Run HPX (this copies the args structure)
  return hpx_run(_main, &args, sizeof(args));
}
