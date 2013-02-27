
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Library Unit Test Harness - Threads (Stage 1)
  04_thread1.c

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
  TEST: thread creation & initialization
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_create)
{
  hpx_context_t * ctx = NULL;
  hpx_thread_t * th = NULL;
  
  ctx = hpx_ctx_create();
  ck_assert_msg(ctx != NULL, "Could not create context");

  th = hpx_thread_create(ctx);
  ck_assert_msg(th != NULL, "Could not create thread");

  hpx_thread_destroy(th);
  hpx_ctx_destroy(ctx);

  th = NULL;
  ctx = NULL;
} 
END_TEST

