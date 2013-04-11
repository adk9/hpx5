
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

struct {
  unsigned char * buf;
  uint64_t elapsed_ts;
  uint64_t min_ts;
  uint64_t max_ts;
} thread_buf;


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
  TEST HELPER: Worker Function for multi_thread_set.
 --------------------------------------------------------------------
*/

void multi_thread_set_worker(void * ptr) {
  char * my_idx = (char *) ptr;
  uint32_t buf_idx;
  int idx;

  for (idx = 0; idx < 256; idx++) {
    buf_idx = (uint32_t) idx + ((uint64_t) my_idx - (uint64_t) thread_buf.buf);
    thread_buf.buf[buf_idx] = (unsigned char) idx;
  }
}


/*
 --------------------------------------------------------------------
  TEST HELPER: Test Runner for multi_thread_set
 --------------------------------------------------------------------
*/

void run_multi_thread_set(uint64_t mflags, uint32_t th_cnt) {
  hpx_context_t * ctx;
  hpx_thread_t ** ths;
  char msg[128];
  uint32_t buf_idx;
  int idx;

  memset(&thread_buf, 0, sizeof(thread_buf));

  /* get a thread context */
  ctx = hpx_ctx_create(mflags);
  ck_assert_msg(ctx != NULL, "Could not get a thread context.");

  /* create & init our test data */
  thread_buf.buf = (char *) hpx_alloc(sizeof(char) * th_cnt * 256);
  ck_assert_msg(thread_buf.buf != NULL, "Could not allocate memory for test data.");

  memset(thread_buf.buf, 0, sizeof(char) * th_cnt * 256);

  /* create HPX theads */
  ths = (hpx_thread_t **) hpx_alloc(sizeof(hpx_thread_t *) * th_cnt);
  ck_assert_msg(ths != NULL, "Could not allocate an array to hold thread data.");

  for(idx = 0; idx < th_cnt; idx++) {
    buf_idx = idx * 256;
    ths[idx] = hpx_thread_create(ctx, multi_thread_set_worker, &thread_buf.buf[buf_idx]);
  }

  /* wait until our threads are done */
  for (idx = 0; idx < th_cnt; idx++) {
    hpx_thread_join(ths[idx], NULL);
  }

  /* make sure things got done right */
  for (idx = 0; idx < (th_cnt * 256); idx++) {
    sprintf(msg, "Thread data was not set at index %d (expected %d, got %d).", idx, (idx % 256), thread_buf.buf[idx]);
    ck_assert_msg(thread_buf.buf[idx] == (idx % 256), msg);
  }

  /* clean up */
  for (idx = 0; idx < th_cnt; idx++) {
    hpx_thread_destroy(ths[idx]);
  }

  hpx_free(ths);
  hpx_free(thread_buf.buf);

  hpx_ctx_destroy(ctx);
}


/*
 --------------------------------------------------------------------
  TEST HELPER: Worker Function for multi_thread_set_yield.
 --------------------------------------------------------------------
*/

void multi_thread_set_yield_worker(void * ptr) {
  char * my_idx = (char *) ptr;
  uint32_t buf_idx;
  int idx;

  for (idx = 0; idx < 256; idx++) {
    buf_idx = (uint32_t) idx + ((uint64_t) my_idx - (uint64_t) thread_buf.buf);
    thread_buf.buf[buf_idx] = (unsigned char) idx;
    hpx_thread_yield();
  }
}


/*
 --------------------------------------------------------------------
  TEST HELPER: Test Runner for multi_thread_set_yield
 --------------------------------------------------------------------
*/

void run_multi_thread_set_yield(uint64_t mflags, uint32_t th_cnt) {
  hpx_context_t * ctx;
  hpx_thread_t ** ths;
  char msg[128];
  uint32_t buf_idx;
  int idx;

  memset(&thread_buf, 0, sizeof(thread_buf));

  /* get a thread context */
  ctx = hpx_ctx_create(mflags);
  ck_assert_msg(ctx != NULL, "Could not get a thread context.");

  /* create & init our test data */
  thread_buf.buf = (char *) hpx_alloc(sizeof(char) * th_cnt * 256);
  ck_assert_msg(thread_buf.buf != NULL, "Could not allocate memory for test data.");

  memset(thread_buf.buf, 0, sizeof(char) * th_cnt * 256);

  /* create HPX theads */
  ths = (hpx_thread_t **) hpx_alloc(sizeof(hpx_thread_t *) * th_cnt);
  ck_assert_msg(ths != NULL, "Could not allocate an array to hold thread data.");

  for(idx = 0; idx < th_cnt; idx++) {
    buf_idx = idx * 256;
    ths[idx] = hpx_thread_create(ctx, multi_thread_set_yield_worker, &thread_buf.buf[buf_idx]);
  }

  /* wait until our threads are done */
  for (idx = 0; idx < th_cnt; idx++) {
    hpx_thread_join(ths[idx], NULL);
  }

  /* make sure things got done right */
  for (idx = 0; idx < (th_cnt * 256); idx++) {
    sprintf(msg, "Thread data was not set at index %d (expected %d, got %d).", idx, (idx % 256), thread_buf.buf[idx]);
    ck_assert_msg(thread_buf.buf[idx] == (idx % 256), msg);
  }

  /* clean up */
  for (idx = 0; idx < th_cnt; idx++) {
    hpx_thread_destroy(ths[idx]);
  }

  hpx_free(ths);
  hpx_free(thread_buf.buf);

  hpx_ctx_destroy(ctx);
}



/*
 --------------------------------------------------------------------
  TEST HELPER: Test Runner for thread_args
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

  /* create HPX thead */
  th1 = hpx_thread_create(ctx, thread_counter_arg1_worker, &th_arg);

  /* wait until our thread is done */
  hpx_thread_join(th1, NULL);

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
  TEST HELPER: Test Runner for hpx_thread_strcpy().
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

  /* wait until our threads are done */
  for (idx = 0; idx < th_cnt; idx++) {
    hpx_thread_join(ths[idx], NULL);
  }

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

  /* wait on the thread */
  hpx_thread_join(th, NULL);

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
  printf("RUNNING TEST test_libhpx_thread_strcpy_th1_core1\n  single thread string copy on a singal logical CPU with no switching flags.\n");
  run_thread_strcpy(0, 1, 1, thread_msg1, strlen(thread_msg1));
  printf("DONE\n\n");
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
  printf("RUNNING TEST test_libhpx_thread_strcpy_th1_core1_ext\n  single thread string copy on a single logical CPU, saving extended (FPU) state.\n");
  run_thread_strcpy(HPX_MCTX_SWITCH_EXTENDED, 1, 1, thread_msg1, strlen(thread_msg1));
  printf("DONE\n\n");
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
  printf("RUNNING TEST test_libhpx_thread_strcpy_th1_core1_sig\n   single thread string copy on a single logical CPU, saving the thread signal mask.\n");
  run_thread_strcpy(HPX_MCTX_SWITCH_SIGNALS, 1, 1, thread_msg1, strlen(thread_msg1));
  printf("DONE\n\n");
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
  printf("RUNNING TEST test_libhpx_thread_strcpy_th1_core1_ext_sig\n  single thread string copy on a single logical CPU, saving extended (FPU) state and the signal mask.\n");
  run_thread_strcpy(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, 1, 1, thread_msg1, strlen(thread_msg1));
  printf("DONE\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: get a pointer to the current thread's data with no flags.
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_self_ptr)
{
  printf("RUNNING TEST test_libhpx_thread_self_ptr\n  get a pointer to the current thread's data with no switching flags.\n");
  run_thread_self_get_ptr(0);
  printf("DONE\n\n");
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
  printf("RUNNING TEST test_libhpx_thread_self_ptr_ext\n  get a pointer to the current thread's data, saving extended (FPU) state.\n");
  run_thread_self_get_ptr(HPX_MCTX_SWITCH_EXTENDED);
  printf("DONE\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: get a pointer to the current thread's data, saving signals.
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_self_ptr_sig)
{
  printf("RUNNING TEST test_libhpx_thread_self_ptr_sig\n  get a pointer to the current thread's data, saving the thread signal mask.\n");
  run_thread_self_get_ptr(HPX_MCTX_SWITCH_SIGNALS);
  printf("DONE\n\n");
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
  printf("RUNNING TEST test_libhpx_thread_self_ptr_ext_sig\n  get a pointer to the current thread's data, saving extended (FPU) state and the thread signal mask.\n");
  run_thread_self_get_ptr(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS);
  printf("DONE\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: pass an argument into threads with no flags.
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_args)
{
  printf("RUNNING TEST test_libhpx_thread_args\n  pass an argument to a thread with no switching flags.\n");
  run_thread_args(0);
  printf("DONE\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: pass an argument into threads, saving extended state
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_args_ext)
{
  printf("RUNNING TEST test_libhpx_thread_args_ext\n  pass an argument to a thread, saving extended (FPU) state.\n");
  run_thread_args(HPX_MCTX_SWITCH_EXTENDED);
  printf("DONE\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: pass an argument into threads, saving signals.
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_args_sig)
{
  printf("RUNNING TEST test_libhpx_thread_args_sig\n  pass an argument to a thread, saving the signal mask.\n");
  run_thread_args(HPX_MCTX_SWITCH_SIGNALS);
  printf("DONE\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: pass an argument into threads, saving extended state and
  signals.
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_args_ext_sig)
{
  printf("RUNNING TEST test_libhpx_thread_args_ext_sig\n  pass an argument to a thread, saving extended (FPU) state and the thread signal mask.\n");
  run_thread_args(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS);
  printf("DONE\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: create two threads on each core, with no flags
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_multi_thread_set_x2)
{
  run_multi_thread_set(0, hpx_kthread_get_cores() * 2);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: create two threads on each core, saving extended state
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_multi_thread_set_x2_ext)
{
  run_multi_thread_set(HPX_MCTX_SWITCH_EXTENDED, hpx_kthread_get_cores() * 2);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: create two threads on each core, saving signals
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_multi_thread_set_x2_sig)
{
  run_multi_thread_set(HPX_MCTX_SWITCH_SIGNALS, hpx_kthread_get_cores() * 2);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: create two threads on each core, saving extended state
  and signals.
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_multi_thread_set_x2_ext_sig)
{
  run_multi_thread_set(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, hpx_kthread_get_cores() * 2);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: create 32 threads on each core, with no flags
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_multi_thread_set_x32)
{
  run_multi_thread_set(0, hpx_kthread_get_cores() * 32);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: create 32 threads on each core, saving extended state
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_multi_thread_set_x32_ext)
{
  run_multi_thread_set(HPX_MCTX_SWITCH_EXTENDED, hpx_kthread_get_cores() * 32);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: create 32 threads on each core, saving signals
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_multi_thread_set_x32_sig)
{
  run_multi_thread_set(HPX_MCTX_SWITCH_SIGNALS, hpx_kthread_get_cores() * 32);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: create 32 threads on each core, saving extended state and
  signals.
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_multi_thread_set_x32_ext_sig)
{
  run_multi_thread_set(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, hpx_kthread_get_cores() * 32);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: Futures
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_lco_futures)
{
  hpx_future_t fut1;
  hpx_future_t fut2;
  char msg[128];
  int * xp;
  int x = 73;

  /* initialize Future 1 */
  hpx_lco_future_init(&fut1);
  sprintf(msg, "Future 1 was not initialized in an UNSET state (expected %d, got %d).", HPX_LCO_FUTURE_UNSET, fut1.state);
  ck_assert_msg(fut1.state == HPX_LCO_FUTURE_UNSET, msg);

  /* set Future 1 to NULL */
  fut1.value = NULL;
  hpx_lco_future_set(&fut1);
  sprintf(msg, "Future 1 was not set (expected %d, got %d).", HPX_LCO_FUTURE_SET, fut1.state);
  ck_assert_msg(fut1.state == HPX_LCO_FUTURE_SET, msg);
  
  sprintf(msg, "Future 1 was set with an incorrect value (expected NULL, got %ld).", (uint64_t) fut1.value);
  ck_assert_msg(fut1.value == NULL, msg);

  /* initialize Future 2 */
  hpx_lco_future_init(&fut2);
  sprintf(msg, "Future 2 was not initialized in an UNSET state (expected %d, got %d).", HPX_LCO_FUTURE_UNSET, fut2.state);
  ck_assert_msg(fut2.state == HPX_LCO_FUTURE_UNSET, msg);

  /* set Future 2 to a value */
  fut2.value = &x;
  hpx_lco_future_set(&fut2);
  sprintf(msg, "Future 2 was not set (expected %d, got %d).", HPX_LCO_FUTURE_SET, fut2.state);
  ck_assert_msg(fut2.state == HPX_LCO_FUTURE_SET, msg);
  
  xp = (int *) fut2.value;
  sprintf(msg, "Future 2 was set with an incorrect value (expected 73, got %d).", *xp);
  ck_assert_msg(*xp == 73, msg);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: create 1,000 HPX threads on each compute core, saving
  extended (FPU) state and the signal mask.  
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_multi_thread_set_hardcore1000)
{
  printf("RUNNING TEST test_libhpx_thread_multi_thread_set_hardcore1000\n  create 1,000 threads per core, saving extended (FPU) state and the thread signal mask.\n");
  run_multi_thread_set(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, hpx_kthread_get_cores() * 1000);
  printf("DONE\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: create 5,000 HPX threads on each compute core, saving
  extended (FPU) state and the signal mask.  
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_multi_thread_set_hardcore5000)
{
  printf("RUNNING TEST test_libhpx_thread_multi_thread_set_hardcore5000\n  create 5,000 threads per core, saving extended (FPU) state and the thread signal mask.\n");
  run_multi_thread_set(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, hpx_kthread_get_cores() * 5000);
  printf("DONE\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: run one thread that yields to itself, with no flags
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_multi_thread_set_yield1)
{
  printf("RUNNING TEST test_libhpx_thread_multi_thread_set_yield1\n  run one thread that yields to itself, with no switching flags.\n");
  run_multi_thread_set_yield(0, 1);
  printf("DONE\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: run two threads that yield to one another, with no flags
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_multi_thread_set_yield2)
{
  printf("RUNNING TEST test_libhpx_thread_multi_thread_set_yield2\n  run two threads that yield to one another, with no switching flags.\n");
  run_multi_thread_set_yield(0, 2);
  printf("DONE\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: run two threads per core that yield to one another, with 
  no flags
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_multi_thread_set_yield_x2)
{
  printf("RUNNING TEST test_libhpx_thread_multi_thread_set_yield_x2\n  run two threads per core that yield to one another, with no switching flags.\n");
  run_multi_thread_set_yield(0, hpx_kthread_get_cores() * 2);
  printf("DONE\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: run 32 threads per core that yield to one another, with 
  no flags
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_multi_thread_set_yield_x32)
{
  printf("RUNNING TEST test_libhpx_thread_multi_thread_set_yield_x32\n  run 32 threads per core that yield to one another, with no switching flags.\n");
  run_multi_thread_set_yield(0, hpx_kthread_get_cores() * 32);
  printf("DONE\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: run 1,000 threads per core that yield to one another, 
  saving extended (FPU) state and the thread signal mask.
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_multi_thread_set_yield_hardcore1000)
{
  printf("RUNNING TEST test_libhpx_thread_multi_thread_set_yield_hardcore1000\n  run 1,000 threads per core that yield to one another, with no switching flags.\n");
  run_multi_thread_set_yield(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, hpx_kthread_get_cores() * 1000);
  printf("DONE\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: run 5,000 threads per core that yield to one another, 
  saving extended (FPU) state and the thread signal mask.
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_multi_thread_set_yield_hardcore5000)
{
  printf("RUNNING TEST test_libhpx_thread_multi_thread_set_yield_hardcore5000\n  run 5,000 threads per core that yield to one another, with no switching flags.\n");
  run_multi_thread_set_yield(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, hpx_kthread_get_cores() * 5000);
  printf("DONE\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: run 50,000 threads per core that yield to one another, 
  saving extended (FPU) state and the thread signal mask.
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_multi_thread_set_yield_hardcore10000)
{
  printf("RUNNING TEST test_libhpx_thread_multi_thread_set_yield_hardcore10000\n  run 10,000 threads per core that yield to one another, with no switching flags.\n");
  run_multi_thread_set_yield(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, hpx_kthread_get_cores() * 10000);
  printf("DONE\n\n");
}
END_TEST


