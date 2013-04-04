
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


#include <string.h>
#include "hpx_thread.h"


/*
 --------------------------------------------------------------------
  TEST DATA
 --------------------------------------------------------------------
*/

char thread_msg1[] = "The open steppe, fleet horse, falcons at your wrist, and the wind in your hair.";
char thread_msg2[] = "To crush your enemies, see them driven before you, and to hear the lamentation of their women.";

char * thread_msgbuf;
hpx_thread_t * th1;


/*
 --------------------------------------------------------------------
  TEST HELPER: Worker Function for run_thread_yield_counter().
 --------------------------------------------------------------------
*/

void thread_yield_counter_worker(void) {
  hpx_thread_t * th = NULL;
  uint8_t th_state = 0;

  strcpy(thread_msgbuf, thread_msg1);

  th1 = hpx_thread_self();
}


/*
 --------------------------------------------------------------------
  TEST HELPER: Test Runner for hpx_thread_yield().
 --------------------------------------------------------------------
*/

void run_thread_yield_counter(uint64_t mflags, uint64_t th_cnt, uint64_t core_cnt, char * orig_msg, size_t msg_len) {
  hpx_context_t * ctx;
  hpx_thread_t * ths[th_cnt];
  uint64_t idx;
  char msg[128 + msg_len + 1];  // only l337 h4x0rz can get around this lol

  /* get a thread context */
  ctx = hpx_ctx_create(mflags);
  ck_assert_msg(ctx != NULL, "Could not get a thread context.");

  /* initialize our message buffer */
  thread_msgbuf = (char *) hpx_alloc(sizeof(char) * msg_len);
  ck_assert_msg(thread_msgbuf != NULL, "Could not initialize thread message buffer.");

  memset(thread_msgbuf, 0, sizeof(char) * msg_len);

  /* create HPX theads */
  for (idx = 0; idx < th_cnt; idx++) {
    ths[idx] = hpx_thread_create(ctx, thread_yield_counter_worker, NULL);
  }

  sleep(5);

  /* clean up */
  for (idx = 0; idx < th_cnt; idx++) {
    hpx_thread_destroy(ths[idx]);
  }

  /* make sure our string got copied */
  sprintf(msg, "Thread message was not copied (expected \"%s\", got \"%s\").", orig_msg, thread_msgbuf);
  ck_assert_msg(strcmp(thread_msgbuf, orig_msg) == 0, msg);

  hpx_free(thread_msgbuf);

  hpx_ctx_destroy(ctx);

  ck_assert_msg(th1 != NULL, "Could not get a pointer to the current context's TLS data.");
}


/*
 --------------------------------------------------------------------
  TEST: thread yields on a single logical CPU.
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_yield1_core1)
{
  run_thread_yield_counter(0, 1, 1, thread_msg1, strlen(thread_msg1));
}
END_TEST
