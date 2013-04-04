
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
char * test_buf;

char * thread_msgbuf;
hpx_thread_t * th_self;
void * thread_arg;
int thread_counter;


/*
 --------------------------------------------------------------------
  TEST HELPER: Worker Function for run_thread_counter_arg1
 --------------------------------------------------------------------
*/

void thread_counter_arg1_worker(void * a) {
  int * a_ptr;

  thread_arg = a;
  a_ptr = (int *) a;
  thread_counter += *a_ptr;
}


/*
 --------------------------------------------------------------------
  TEST HELPER: Worker Function for run_thread_self_ptr
 --------------------------------------------------------------------
*/

void thread_self_ptr_worker(void) {
  th_self = hpx_thread_self();
}


/*
 --------------------------------------------------------------------
  TEST HELPER: Worker Function for run_thread_strcpy.
 --------------------------------------------------------------------
*/

void thread_strcpy_worker(void) {
  strcpy(thread_msgbuf, thread_msg1);
}



/*
 --------------------------------------------------------------------
  TEST HELPER: Test Runner for thread_counters
 --------------------------------------------------------------------
*/

void run_thread_args(uint64_t mflags) {
  hpx_context_t * ctx;
  hpx_thread_t * th1;
  char msg[128];
  int * th_arg_ptr;
  int th_arg = 8473;

  /* get a thread context */
  ctx = hpx_ctx_create(mflags);
  ck_assert_msg(ctx != NULL, "Could not get a thread context.");

  /* create HPX theads */
  th1 = hpx_thread_create(ctx, thread_counter_arg1_worker, &th_arg);

  /* this hobbit is tricksy and false, but we aren't testing control objects yet */
  sleep(5);

  /* make sure we got the right arguments */
  th_arg_ptr = thread_arg;
  sprintf(msg, "Thread argument is not correct (expected 8473, got %d).", *th_arg_ptr);
  ck_assert_msg(*th_arg_ptr == 8473, msg);

  /* clean up */
  hpx_thread_destroy(th1);  

  hpx_ctx_destroy(ctx);
}


/*
 --------------------------------------------------------------------
  TEST HELPER: Test Runner for hpx_thread_yield().
 --------------------------------------------------------------------
*/

void run_thread_strcpy(uint64_t mflags, uint64_t th_cnt, uint64_t core_cnt, char * orig_msg, size_t msg_len) {
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
    ths[idx] = hpx_thread_create(ctx, thread_strcpy_worker, 0);
  }

  /* this hobbit is tricksy and false, but we aren't testing control objects yet */
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
}


/*
 --------------------------------------------------------------------
  TEST HELPER: Test Runner for thread_self_get_ptr
 --------------------------------------------------------------------
*/

void run_thread_self_get_ptr(uint64_t mflags) {
  hpx_context_t * ctx;
  hpx_thread_t * th;
  hpx_thread_id_t id1;
  hpx_thread_id_t id2;
  hpx_kthread_t * kth1;
  hpx_kthread_t * kth2;
  char msg[128];

  th_self = NULL;

  /* get a thread context */
  ctx = hpx_ctx_create(mflags);
  ck_assert_msg(ctx != NULL, "Could not get a thread context.");

  /* create an HPX thead */
  th = hpx_thread_create(ctx, thread_self_ptr_worker, 0);

  id1 = th->tid;
  kth1 = th->kth;

  /* more tricks */
  sleep(2);

  /* make sure we have something good */
  ck_assert_msg(th_self != NULL, "Could not get a pointer to a thread's TLS data.");

  /* make sure it's actually the data we want */
  id2 = th_self->tid;
  ck_assert_msg(id1 == id2, "Thread IDs do not match (expected %ld, got %ld).", id1, id2);  
  
  kth2 = th_self->kth;
  ck_assert_msg(kth1 == kth2, "Thread kernel contexts do not match (expected %ld, got %ld).", (uint64_t) kth1, (uint64_t) kth2);

  /* clean up */
  hpx_thread_destroy(th);

  hpx_free(thread_msgbuf);
  hpx_ctx_destroy(ctx);
}



/*
 --------------------------------------------------------------------
  TEST: single thread string copy on a single logical CPU with no
  switching flags.
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_strcpy_th1_core1)
{
  run_thread_strcpy(0, 1, 1, thread_msg1, strlen(thread_msg1));
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: single thread string copy on a single logical CPU, saving
  extended state.
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_strcpy_th1_core1_ext)
{
  run_thread_strcpy(HPX_MCTX_SWITCH_EXTENDED, 1, 1, thread_msg1, strlen(thread_msg1));
}
END_TEST

/*
 --------------------------------------------------------------------
  TEST: single thread string copy on a single logical CPU, saving
  signals.
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_strcpy_th1_core1_sig)
{
  run_thread_strcpy(HPX_MCTX_SWITCH_SIGNALS, 1, 1, thread_msg1, strlen(thread_msg1));
}
END_TEST

/*
 --------------------------------------------------------------------
  TEST: single thread string copy on a single logical CPU, saving
  extended state and signals.
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_strcpy_th1_core1_ext_sig)
{
  run_thread_strcpy(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, 1, 1, thread_msg1, strlen(thread_msg1));
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: get a pointer to the current thread's data with no flags.
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_self_ptr)
{
  run_thread_self_get_ptr(0);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: get a pointer to the current thread's data, saving
  extended state..
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_self_ptr_ext)
{
  run_thread_self_get_ptr(HPX_MCTX_SWITCH_EXTENDED);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: get a pointer to the current thread's data, saving signals.
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_self_ptr_sig)
{
  run_thread_self_get_ptr(HPX_MCTX_SWITCH_SIGNALS);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: get a pointer to the current thread's data, saving
  extended state and signals.
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_self_ptr_ext_sig)
{
  run_thread_self_get_ptr(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: pass 1, 2, 3, 4, 5, 6, 7, 8, and 8+ arguments into threads
  with no flags.
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_args)
{
  run_thread_args(0);
}
END_TEST
