
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Library Unit Test Harness - Threads (Stage 2)
  08_thread2.c

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


#include "hpx_thread.h"


/*
 --------------------------------------------------------------------
  TEST HELPER: Test Runner for hpx_thread_yield().
 --------------------------------------------------------------------
*/

void run_thread_yield_counter(void) {
  hpx_context_t * ctx;

  /* get a thread context */
  ctx = hpx_ctx_create();
  ck_assert_msg(ctx != NULL, "Could not get a thread context.");

  /* clean up */
  hpx_ctx_destroy(ctx);
}


/*
 --------------------------------------------------------------------
  TEST: thread yields on a single logical CPU.
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_yield1_core1)
{
  run_thread_yield_counter();
}
END_TEST
