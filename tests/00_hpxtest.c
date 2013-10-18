
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
#include "tests.h"

/*
 --------------------------------------------------------------------
  Main
 --------------------------------------------------------------------
*/
int main(int argc, char * argv[]) {
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
  tcase_set_timeout(tc, 1200);

  
  add_09_config(tc);                            /* test configuration */
  add_02_mem(tc);                               /* test memory management */
  add_06_kthread(tc);                           /* test kernel threads,
                                                   NOTE: before ctx tests */
  add_03_ctx(tc);                               /* scheduling ctx management */
  /* add_04_thread1(tc); LD: why not? */        /* threads (stage 1) */
  add_05_queue(tc);                             /* FIFO queues */
  add_10_list(tc);                              /* linked lists */
  add_11_map(tc);                               /* maps */
  add_07_mctx(tc, long_tests);                  /* machine ctx switching */
  add_08_thread2(tc, long_tests, hardcore_tests); /* LCOs, threads (stage 2) */
  add_12_gate(tc);                              /* gates */
  #if HAVE_NETWORK
  add_12_parcelhandler(tc);                     /* parcel handler tests */
  #endif
  if (perf_tests)
    add_98_thread_perf1(tc);

  suite_add_tcase(s, tc);

  SRunner * sr = srunner_create(s);
  srunner_run_all(sr, CK_VERBOSE);

  int failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  
  return (failed == 0) ? 0 : -1;
}
