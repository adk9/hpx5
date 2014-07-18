
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)

  Library Unit Test Harness - Initialization and Cleanup Functions
  01_init.c

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

#include "hpx/hpx.h"
#include "tests.h"

FILE * perf_log;


/*
 --------------------------------------------------------------------
  TEST SUITE FIXTURE: library initialization
 --------------------------------------------------------------------
*/

void hpxtest_core_setup(void) {
  hpx_error_t err;

  /* initialize libhpx */
  err = hpx_init(NULL);
  ck_assert_msg(err == HPX_SUCCESS, "Could not initialize libhpx");

  /* open a performance log file */
  perf_log = fopen("perf.log", "w+");
  ck_assert_msg(perf_log != NULL, "Could not open performance log");

}


/*
 --------------------------------------------------------------------
  TEST SUITE FIXTURE: library cleanup
 --------------------------------------------------------------------
*/

void hpxtest_core_teardown(void) {
  fclose(perf_log);
  hpx_cleanup();
}


