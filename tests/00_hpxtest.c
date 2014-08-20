
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
  tcase_set_timeout(tc, 8000);

  add_02_TestMemAlloc(tc);  
  add_03_TestGlobalMemAlloc(tc);
  add_04_TestMemMove(tc);

  suite_add_tcase(s, tc);

  SRunner * sr = srunner_create(s);
  srunner_run_all(sr, CK_NORMAL);

  int failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  
  return (failed == 0) ? 0 : -1;
}
