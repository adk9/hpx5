
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


#include "hpx/hpx.h"

void * __thread_test_func1(void) {
  return NULL;
}


/*
 --------------------------------------------------------------------
  TEST: thread creation & initialization
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_create)
{
  hpx_context_t * ctx = NULL;
  hpx_future_t * th = NULL;
  hpx_thread_state_t state;  
  char msg[128];

  ctx = hpx_ctx_create(0);
  ck_assert_msg(ctx != NULL, "Could not create context");

  th = hpx_thread_create(ctx, 0, __thread_test_func1, 0, NULL);
  ck_assert_msg(th != NULL, "Could not create thread");

  //  state = hpx_thread_get_state(th);
  //  sprintf(msg, "New thread has an invalid queuing state (expected %d, got %d)", HPX_THREAD_STATE_PENDING, (int) state);
  //  ck_assert_msg(state == HPX_THREAD_STATE_PENDING, msg);

  //  hpx_thread_destroy(th);
  hpx_thread_wait(th);
  hpx_ctx_destroy(ctx);

  th = NULL;
  ctx = NULL;
} 
END_TEST

