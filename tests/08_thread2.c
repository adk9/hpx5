
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
#include "hpx/hpx.h"
#include "tests.h"


/*
 --------------------------------------------------------------------
  TEST DATA
 --------------------------------------------------------------------
*/

static char* messages[] = {
  "The open steppe, fleet horse, falcons at your wrist, and the wind in your "
  "hair.",
  "To crush your enemies, see them driven before you, and to hear the "
  "lamentation of their women."
};

static char *test_buf;

static char *thread_msgbuf;
static hpx_thread_t *th_self;
static void *thread_arg;
static int thread_counter;

static unsigned char *thread_buf;


/*
 --------------------------------------------------------------------
  TEST HELPER: Worker Function for run_thread_counter_arg1
 --------------------------------------------------------------------
*/

static void thread_counter_arg1_worker(void * a) {
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

static void thread_self_ptr_worker(void *args) {
  th_self = hpx_thread_self();
}


/*
 --------------------------------------------------------------------
  TEST HELPER: Worker Function for run_thread_strcpy.
 --------------------------------------------------------------------
*/

static void thread_strcpy_worker(void *args) {
  strcpy(thread_msgbuf, messages[0]);
}


/*
 --------------------------------------------------------------------
  TEST HELPER: Worker Function for multi_thread_set.
 --------------------------------------------------------------------
*/

static void multi_thread_set_worker(void * ptr) {
  char * my_idx = (char *) ptr;
  uint32_t buf_idx;
  int idx;

  for (idx = 0; idx < 256; idx++) {
    buf_idx = (uint32_t) idx + ((uint64_t) my_idx - (uint64_t) thread_buf);
    thread_buf[buf_idx] = (unsigned char) idx;
  }
}


/*
 --------------------------------------------------------------------
  TEST HELPER: Test Runner for multi_thread_set
 --------------------------------------------------------------------
*/

static void run_multi_thread_set(uint64_t mflags, uint32_t core_cnt, uint32_t th_cnt) {
  hpx_context_t * ctx;
  hpx_future_t ** fts;
  hpx_config_t cfg;
  char msg[128];
  uint32_t buf_idx;
  int idx;

  /* init our config */
  hpx_config_init(&cfg);
  hpx_config_set_switch_flags(&cfg, mflags);

  if (core_cnt > 0) {
    hpx_config_set_cores(&cfg, core_cnt);
  }

  /* get a thread context */
  ctx = hpx_ctx_create(&cfg);
  ck_assert_msg(ctx != NULL, "Could not get a thread context.");

  /* create & init our test data */
  thread_buf = hpx_alloc(sizeof(thread_buf[0]) * th_cnt * 256);
  ck_assert_msg(thread_buf != NULL, "Could not allocate memory for test data.");

  memset(thread_buf, 0, sizeof(char) * th_cnt * 256);

  /* create HPX theads */
  fts = (hpx_future_t **) hpx_alloc(sizeof(hpx_future_t *) * th_cnt);
  ck_assert_msg(fts != NULL, "Could not allocate an array to hold thread data.");

  for(idx = 0; idx < th_cnt; idx++) {
    buf_idx = idx * 256;
    hpx_thread_create(ctx, 0, multi_thread_set_worker, &thread_buf[buf_idx],
                      &fts[idx], NULL);
  }

  /* wait until our threads are done */
  for (idx = 0; idx < th_cnt; idx++) {
    hpx_thread_wait(fts[idx]);
  }

  /* make sure things got done right */
  for (idx = 0; idx < (th_cnt * 256); idx++) {
    sprintf(msg, "Thread data was not set at index %d (expected %d, got %d).", idx, (idx % 256), thread_buf[idx]);
    ck_assert_msg(thread_buf[idx] == (idx % 256), msg);
  }

  /* clean up */
  hpx_free(fts);
  hpx_free(thread_buf);

  hpx_ctx_destroy(ctx);
}


/*
 --------------------------------------------------------------------
  TEST HELPER: Worker Function for multi_thread_set_yield.
 --------------------------------------------------------------------
*/

static void multi_thread_set_yield_worker(void * ptr) {
  char * my_idx = (char *) ptr;
  uint32_t buf_idx;
  int idx;

  for (idx = 0; idx < 256; idx++) {
    buf_idx = (uint32_t) idx + ((uint64_t) my_idx - (uint64_t) thread_buf);
    thread_buf[buf_idx] = (unsigned char) idx;
    hpx_thread_yield();
  }
}


/*
 --------------------------------------------------------------------
  TEST HELPER: Test Runner for multi_thread_set_yield
 --------------------------------------------------------------------
*/

static void run_multi_thread_set_yield(uint64_t mflags, uint32_t core_cnt, uint32_t th_cnt) {
  hpx_context_t * ctx;
  hpx_future_t ** ths;
  hpx_config_t cfg;
  char msg[128];
  uint32_t buf_idx;
  int idx;

  /* init our config */
  hpx_config_init(&cfg);
  hpx_config_set_switch_flags(&cfg, mflags);

  if (core_cnt > 0) {
    hpx_config_set_cores(&cfg, core_cnt);
  }

  /* get a thread context */
  ctx = hpx_ctx_create(&cfg);
  ck_assert_msg(ctx != NULL, "Could not get a thread context.");

  /* create & init our test data */
  thread_buf = hpx_alloc(sizeof(thread_buf[0]) * th_cnt * 256);
  ck_assert_msg(thread_buf != NULL, "Could not allocate memory for test data.");

  memset(thread_buf, 0, sizeof(thread_buf[0]) * th_cnt * 256);

  /* create HPX theads */
  ths = (hpx_future_t **) hpx_alloc(sizeof(hpx_future_t *) * th_cnt);
  ck_assert_msg(ths != NULL, "Could not allocate an array to hold thread data.");

  for(idx = 0; idx < th_cnt; idx++) {
    buf_idx = idx * 256;
    hpx_thread_create(ctx, 0, multi_thread_set_yield_worker,
                      &thread_buf[buf_idx], &ths[idx], NULL);
  }

  /* wait until our threads are done */
  for (idx = 0; idx < th_cnt; idx++) {
    hpx_thread_wait(ths[idx]);
  }

  /* make sure things got done right */
  for (idx = 0; idx < (th_cnt * 256); idx++) {
    sprintf(msg, "Thread data was not set at index %d (expected %d, got %d).", idx, (idx % 256), thread_buf[idx]);
    ck_assert_msg(thread_buf[idx] == (idx % 256), msg);
  }

  /* clean up */
  hpx_free(ths);
  hpx_free(thread_buf);

  hpx_ctx_destroy(ctx);
}



/*
 --------------------------------------------------------------------
  TEST HELPER: Test Runner for thread_args
 --------------------------------------------------------------------
*/

static void run_thread_args(uint64_t mflags) {
  hpx_context_t * ctx;
  hpx_future_t * th1;
  hpx_config_t cfg;
  char msg[128];
  int * th_arg_ptr;
  int th_arg = 8473;

  /* init our config */
  hpx_config_init(&cfg);
  hpx_config_set_switch_flags(&cfg, mflags);

  /* get a thread context */
  ctx = hpx_ctx_create(&cfg);
  ck_assert_msg(ctx != NULL, "Could not get a thread context.");

  /* create HPX thead */
  hpx_thread_create(ctx, 0, thread_counter_arg1_worker, &th_arg, &th1, NULL);

  /* wait until our thread is done */
  hpx_thread_wait(th1);

  /* make sure we got the right arguments */
  th_arg_ptr = thread_arg;
  sprintf(msg, "Thread argument is not correct (expected 8473, got %d).", *th_arg_ptr);
  ck_assert_msg(*th_arg_ptr == 8473, msg);

  /* clean up */
  hpx_ctx_destroy(ctx);
}


/*
 --------------------------------------------------------------------
  TEST HELPER: Worker thread for thread stack size testing
 --------------------------------------------------------------------
*/

static void stack_size_worker(void * ptr) {
  volatile float num = 73 / 4;
}


/*
 --------------------------------------------------------------------
  TEST HELPER: Test Runner for hpx_thread_strcpy().
 --------------------------------------------------------------------
*/

static void run_thread_strcpy(uint64_t mflags, uint64_t th_cnt, uint64_t core_cnt, char * orig_msg, size_t msg_len) {
  hpx_context_t * ctx;
  hpx_future_t * ths[th_cnt];
  hpx_config_t cfg;
  uint64_t idx;
  char msg[128 + msg_len + 1];  // only l337 h4x0rz can get around this lol

  /* init our config */
  hpx_config_init(&cfg);
  hpx_config_set_switch_flags(&cfg, mflags);

  /* get a thread context */
  ctx = hpx_ctx_create(&cfg);
  ck_assert_msg(ctx != NULL, "Could not get a thread context.");

  /* initialize our message buffer */
  thread_msgbuf = (char *) hpx_alloc(sizeof(char) * msg_len);
  ck_assert_msg(thread_msgbuf != NULL, "Could not initialize thread message buffer.");

  memset(thread_msgbuf, 0, sizeof(char) * msg_len);

  /* create HPX theads */
  for (idx = 0; idx < th_cnt; idx++) {
    hpx_thread_create(ctx, 0, thread_strcpy_worker, 0, &ths[idx], NULL);
  }

  /* wait until our threads are done */
  for (idx = 0; idx < th_cnt; idx++) {
    hpx_thread_wait(ths[idx]);
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

static void run_thread_self_get_ptr(uint64_t mflags) {
  hpx_context_t * ctx;
  hpx_future_t * th;
  hpx_thread_id_t id1;
  hpx_thread_id_t id2;
  hpx_kthread_t * kth1;
  hpx_kthread_t * kth2;
  hpx_config_t cfg;
  char msg[128];

  th_self = NULL;

  /* init our config */
  hpx_config_init(&cfg);
  hpx_config_set_switch_flags(&cfg, mflags);

  /* get a thread context */
  ctx = hpx_ctx_create(&cfg);
  ck_assert_msg(ctx != NULL, "Could not get a thread context.");

  /* create an HPX thead */
  hpx_thread_create(ctx, 0, thread_self_ptr_worker, 0, &th, NULL);

  //  id1 = th->tid;

  /* wait on the thread */
  hpx_thread_wait(th);

  /* make sure we have something good */
  ck_assert_msg(th_self != NULL, "Could not get a pointer to a thread's TLS data.");

  //  /* make sure it's actually the data we want */
  //  id2 = th_self->tid;
  //  ck_assert_msg(id1 == id2, "Thread IDs do not match (expected %ld, got %ld).", id1, id2);  

  /* clean up */
  //  hpx_free(thread_msgbuf);
  hpx_ctx_destroy(ctx);
}


/*
 --------------------------------------------------------------------
  TEST HELPER: worker function for run_main_hierarchy (level 2)
 --------------------------------------------------------------------
*/

static void main_hierarchy_worker2(void * ptr) {
  hpx_list_node_t * child = NULL;
  hpx_thread_t * parent = (hpx_thread_t *) ptr;
  hpx_thread_t * th = hpx_thread_self();
  int found = 0;
  char msg[128];

  ck_assert_msg(th != NULL, "Thread data is NULL.");
  ck_assert_msg(th->parent != NULL, "Thread has no parent at hierarchy level 2.");

  sprintf(msg, "Thread %" PRIu64
          " has an incorrect parent at hierarchy level 2 (expected %" PRIu64
          ", got %" PRIu64 ")",
          hpx_thread_get_id(th),
          hpx_thread_get_id(parent),
          hpx_thread_get_id(th->parent));
  ck_assert_msg(th->parent == parent, msg);
}


/*
 --------------------------------------------------------------------
  TEST HELPER: worker function for run_main_hierarchy (level 1)
 --------------------------------------------------------------------
*/

static void main_hierarchy_worker1(void * ptr) {
  hpx_list_node_t * child = NULL;
  hpx_thread_t * th = hpx_thread_self();
  hpx_thread_t * parent = (hpx_thread_t *) ptr;
  hpx_future_t * clds[10];
  int found = 0;
  char msg[128];
  uint32_t idx;

  ck_assert_msg(th != NULL, "Thread data is NULL.");
  ck_assert_msg(th->parent != NULL, "Thread has no parent at hierarchy level 1.");

  sprintf(msg, "Thread %" PRIu64 " has an incorrect parent at hierarchy level 1 (expected %" PRIu64 ", got %" PRIu64 ")", hpx_thread_get_id(th), hpx_thread_get_id(parent), hpx_thread_get_id(th->parent));
  ck_assert_msg(th->parent == parent, msg);

  hpx_thread_yield();

  sprintf(msg, "Parent for thread %" PRIu64 " is NULL when spawned from hierarchy level 1.", hpx_thread_get_id(th));
  ck_assert_msg(th->parent != NULL, msg);

  /* create some child threads */
  for (idx = 0; idx < 10; idx++) {
    hpx_thread_create(th->ctx, 0, main_hierarchy_worker2, (void *) th,
                      &clds[idx], NULL);
  }

  /* wait for the children to finish */
  for (idx = 0; idx < 10; idx++) {
    hpx_thread_wait(clds[idx]);
  }
}


/*
 --------------------------------------------------------------------
  TEST HELPER: worker function for run_main_hierarchy (level 0)
 --------------------------------------------------------------------
*/

static void main_hierarchy_worker0(void * ptr) {
  hpx_thread_t * th = hpx_thread_self();
  uint32_t * th_cnt = (uint32_t *) ptr;
  hpx_future_t * clds[*th_cnt];
  char msg[128];
  uint32_t idx;

  hpx_thread_yield();

  sprintf(msg, "Parent for thread %" PRIu64 " is not NULL when spawned from main thread.", hpx_thread_get_id(th));
  ck_assert_msg(th->parent == NULL, msg);

  /* create some child threads */
  for (idx = 0; idx < *th_cnt; idx++) {
    hpx_thread_create(th->ctx, 0, main_hierarchy_worker1, (void *) th,
                      &clds[idx], NULL);
  }

  /* wait for the children to finish */
  for (idx = 0; idx < *th_cnt; idx++) {
    hpx_thread_wait(clds[idx]);
  }
}


/*
 --------------------------------------------------------------------
  TEST HELPER: thread hierarchies
 --------------------------------------------------------------------
*/

static void run_main_hierarchy(uint64_t mflags, uint32_t th_cnt) {
  hpx_context_t * ctx = NULL;
  hpx_config_t cfg;
  hpx_future_t * ths[th_cnt];
  uint32_t idx;

  /* get our config */
  hpx_config_init(&cfg);
  hpx_config_set_switch_flags(&cfg, mflags);

  /* get a thread context */
  ctx = hpx_ctx_create(&cfg);  
  ck_assert_msg(ctx != NULL, "Could not get a thread context.");

  /* create some threads */
  for (idx = 0; idx < th_cnt; idx++) {
    hpx_thread_create(ctx, 0, main_hierarchy_worker0, &th_cnt, &ths[idx], NULL);
  }

  /* wait until the threads are done */
  for (idx = 0; idx < th_cnt; idx++) {
    hpx_thread_wait(ths[idx]);
  }

  /* cleanup */
  hpx_ctx_destroy(ctx);
}


/*
 --------------------------------------------------------------------
  TEST HELPER: Return Value Worker
 --------------------------------------------------------------------
*/

static void return_value_worker(void * ptr) {
  int * x = (int *) hpx_alloc(sizeof(int));
  ck_assert_msg(x != NULL, "Could not allocate a return value.");

  *x = 73;
  hpx_thread_exit((void *) x);
}


/*
 --------------------------------------------------------------------
  TEST HELPER: Return Value Runner
 --------------------------------------------------------------------
*/

static void run_return_value(uint64_t mflags, uint32_t core_cnt, uint64_t th_cnt) {
  hpx_context_t * ctx;
  hpx_future_t * ths[th_cnt];
  hpx_config_t cfg;
  uint64_t idx;
  int * retval;
  char msg[128];

  /* get our configuration */
  hpx_config_init(&cfg);
  hpx_config_set_switch_flags(&cfg, mflags);

  if (core_cnt > 0) {
    hpx_config_set_cores(&cfg, core_cnt);
  }

  /* get our thread context */
  ctx = hpx_ctx_create(&cfg);
  ck_assert_msg(ctx != NULL, "Could not get a thread context.");

  /* create threads */
  for (idx = 0; idx < th_cnt; idx++) {
    hpx_thread_create(ctx, 0, return_value_worker, NULL, &ths[idx], NULL);
    ck_assert_msg(ths[idx] != NULL, "Could not create thread.");
  }

  /* wait for threads to finish */
  for (idx = 0; idx < th_cnt; idx++) {
    hpx_thread_wait(ths[idx]);

    retval = hpx_lco_future_get_value(ths[idx]);
    sprintf(msg, "Return value is incorrect (expected 73, got %d).", *retval);
    ck_assert_msg((int) *retval == 73, msg);
  }

  /* cleanup */
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
  run_thread_strcpy(0, 1, 1, messages[0], strlen(messages[0]));
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
  run_thread_strcpy(HPX_MCTX_SWITCH_EXTENDED, 1, 1, messages[0], strlen(messages[0]));
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
  run_thread_strcpy(HPX_MCTX_SWITCH_SIGNALS, 1, 1, messages[0], strlen(messages[0]));
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
  run_thread_strcpy(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, 1, 1, messages[0], strlen(messages[0]));
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
  run_multi_thread_set(0, 0, hpx_kthread_get_cores() * 2);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: create two threads on each core, saving extended state
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_multi_thread_set_x2_ext)
{
  run_multi_thread_set(HPX_MCTX_SWITCH_EXTENDED, 0, hpx_kthread_get_cores() * 2);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: create two threads on each core, saving signals
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_multi_thread_set_x2_sig)
{
  run_multi_thread_set(HPX_MCTX_SWITCH_SIGNALS, 0, hpx_kthread_get_cores() * 2);
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
  run_multi_thread_set(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, 0, hpx_kthread_get_cores() * 2);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: create 32 threads on each core, with no flags
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_multi_thread_set_x32)
{
  run_multi_thread_set(0, 0, hpx_kthread_get_cores() * 32);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: create 32 threads on each core, saving extended state
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_multi_thread_set_x32_ext)
{
  run_multi_thread_set(HPX_MCTX_SWITCH_EXTENDED, 0, hpx_kthread_get_cores() * 32);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: create 32 threads on each core, saving signals
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_multi_thread_set_x32_sig)
{
  run_multi_thread_set(HPX_MCTX_SWITCH_SIGNALS, 0, hpx_kthread_get_cores() * 32);
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
  run_multi_thread_set(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, 0, hpx_kthread_get_cores() * 32);
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
  hpx_future_t fut3;
  char msg[128];
  int * xp;
  int x = 73;

  /* initialize Future 1 */
  hpx_lco_future_init(&fut1);
  sprintf(msg, "Future 1 was not initialized in an UNSET state (expected 0, got %" PRIu64 ").", hpx_lco_future_get_state(&fut1));
  ck_assert_msg(!(hpx_lco_future_get_state(&fut1) & HPX_LCO_FUTURE_SETMASK), msg);

  /* set Future 1 to NULL */
  //  fut1.value = NULL;
  hpx_lco_future_set_state(&fut1);
  sprintf(msg, "Future 1 was not set (expected %ld, got %" PRIu64 ").", HPX_LCO_FUTURE_SETMASK, hpx_lco_future_get_state(&fut1));
  ck_assert_msg((hpx_lco_future_get_state(&fut1) & HPX_LCO_FUTURE_SETMASK) == HPX_LCO_FUTURE_SETMASK, msg);
  
  sprintf(msg, "Future 1 was set with an incorrect value (expected NULL, got %" PRIu64 ").", (uint64_t) hpx_lco_future_get_value(&fut1));
  ck_assert_msg(hpx_lco_future_get_value(&fut1) == NULL, msg);

  /* initialize Future 2 */
  hpx_lco_future_init(&fut2);
  sprintf(msg, "Future 2 was not initialized in an UNSET state (expected 0, got %" PRIu64 ").", hpx_lco_future_get_state(&fut2));
  ck_assert_msg(!(hpx_lco_future_get_state(&fut2) & HPX_LCO_FUTURE_SETMASK), msg);

  /* set Future 2 to a value */
  hpx_lco_future_set_value(&fut2, &x);

  xp = (int *) hpx_lco_future_get_value(&fut2);
  sprintf(msg, "Future 2 was set with an incorrect value (expected 73, got %d).", *xp);
  ck_assert_msg(*xp == 73, msg);

  /* initialize future 3 */
  hpx_lco_future_init(&fut3);
  
  sprintf(msg, "Future 3 was not initialized in an UNSET state (expected 0, got %" PRIu64 ").", hpx_lco_future_get_state(&fut3));
  ck_assert_msg(!(hpx_lco_future_get_state(&fut3) & HPX_LCO_FUTURE_SETMASK), msg);

  sprintf(msg, "Future 3 was not initialized with a NULL value (got %ld).", (unsigned long) hpx_lco_future_get_value(&fut3));
  ck_assert_msg(hpx_lco_future_get_value(&fut3) == NULL, msg);

  ck_assert_msg(hpx_lco_future_isset(&fut3) == false, "Future 3 is not in an UNSET state.");

  /* set a value on future 3 */
  hpx_lco_future_set_value(&fut3, (void *) 73);

  sprintf(msg, "Future 3 was not set to the correct value (expected %d, got %ld).", 73, (unsigned long) hpx_lco_future_get_value(&fut3));
  ck_assert_msg(hpx_lco_future_get_value(&fut3) == (void *) 73, msg);

  ck_assert_msg(hpx_lco_future_isset(&fut3) == false, "Future 3 is not in an UNSET state.");

  /* set all values on future 3 */
  hpx_lco_future_set(&fut3, 294, (void *) 73);
  sprintf(msg, "Future 3 was not set (expected %ld, got %" PRIu64 ").", (HPX_LCO_FUTURE_SETMASK + 294), hpx_lco_future_get_state(&fut3));
  ck_assert_msg((hpx_lco_future_get_state(&fut3) & HPX_LCO_FUTURE_SETMASK) == HPX_LCO_FUTURE_SETMASK, msg);

  /* clean up */
  hpx_lco_future_destroy(&fut1);
  hpx_lco_future_destroy(&fut2);
  hpx_lco_future_destroy(&fut3);
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
  run_multi_thread_set(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, 0, hpx_kthread_get_cores() * 1000);
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
  run_multi_thread_set(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, 0, hpx_kthread_get_cores() * 5000);
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
  run_multi_thread_set_yield(0, 0, 1);
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
  run_multi_thread_set_yield(0, 0, 2);
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
  run_multi_thread_set_yield(0, 0, hpx_kthread_get_cores() * 2);
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
  run_multi_thread_set_yield(0, 0, hpx_kthread_get_cores() * 32);
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
  run_multi_thread_set_yield(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, 0, hpx_kthread_get_cores() * 1000);
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
  run_multi_thread_set_yield(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, 0, hpx_kthread_get_cores() * 5000);
  printf("DONE\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: run 10,000 threads per core that yield to one another, 
  saving extended (FPU) state and the thread signal mask.
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_multi_thread_set_yield_hardcore10000)
{
  printf("RUNNING TEST test_libhpx_thread_multi_thread_set_yield_hardcore10000\n  run 10,000 threads per core that yield to one another, with no switching flags.\n");
  run_multi_thread_set_yield(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, 0, hpx_kthread_get_cores() * 10000);
  printf("DONE\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: run 5,000 threads on one core, saving extended (FPU) 
  state and the thread signal mask.
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_multi_thread_set_yield_1core_5000)
{
  printf("RUNNING TEST test_libhpx_thread_multi_thread_set_yield_1core_5000\n  run 5,000 threads that yield to one another on one core, with all switching flags set.\n");
  run_multi_thread_set_yield(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, 1, 5000);
  printf("DONE\n\n");
}
END_TEST

/*
 --------------------------------------------------------------------
  TEST: run 5,000 threads on 2 cores, saving extended (FPU) state
  and the thread signal mask.
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_multi_thread_set_yield_2core_5000)
{
  printf("RUNNING TEST test_libhpx_thread_multi_thread_set_yield_2core_5000\n  run 5,000 threads that yield to one another on two cores, with all switching flags set.\n");
  run_multi_thread_set_yield(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, 2, 5000);
  printf("DONE\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: run 5,000 threads on 1024 cores, saving extended (FPU) 
  state and the thread signal mask.
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_multi_thread_set_yield_1024core_5000)
{
  printf("RUNNING TEST test_libhpx_thread_multi_thread_set_yield_1024core_5000\n  run 5,000 threads that yield to one another on 1,024 cores, with all switching flags set.\n");
  run_multi_thread_set_yield(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, 1024, 5000);
  printf("DONE\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: thread stack size
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_stack_size_verify)
{
  hpx_context_t * ctx = NULL;
  hpx_future_t * th = NULL;
  hpx_config_t cfg;
  char msg[128];

  /* set a certain stack size in our configuration */
  hpx_config_init(&cfg);
  hpx_config_set_thread_stack_size(&cfg, 65536);

  /* get a thread context */
  ctx = hpx_ctx_create(&cfg);
  ck_assert_msg(ctx != NULL, "Could not get a thread context.");

  /* create a thread */
  hpx_thread_create(ctx, 0, stack_size_worker, NULL, &th, NULL);
  ck_assert_msg(th != NULL, "Could not create a thread.");

  /* wait for the thread to finish */
  //  hpx_thread_join(th, NULL);

  /* verify it was created with the correct size */
  //  sprintf(msg, "Thread was not created with the correct stack size (expected 65536, got %d).", (int) th->reuse->ss);
  //  ck_assert_msg(th->reuse->ss == 65536, msg);

  /* cleanup */
  hpx_ctx_destroy(ctx);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: create some HPX threads from the main thread and make sure
  their parent <--> child relationships are set correctly, with no
  switching flags.
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_main_hierarchy)
{
  run_main_hierarchy(0, 10);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: create some HPX threads from the main thread and make sure
  their parent <--> child relationships are set correctly, saving
  extended (FPU) state.
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_main_hierarchy_ext)
{
  run_main_hierarchy(HPX_MCTX_SWITCH_EXTENDED, 10);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: create some HPX threads from the main thread and make sure
  their parent <--> child relationships are set correctly, saving
  signals.
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_main_hierarchy_sig)
{
  run_main_hierarchy(HPX_MCTX_SWITCH_SIGNALS, 10);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: create some HPX threads from the main thread and make sure
  their parent <--> child relationships are set correctly, saving
  extended (FPU) state and signals.
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_main_hierarchy_ext_sig)
{
  run_main_hierarchy(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, 10);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: create 1,000 threads and make sure they set the correct
  return value.
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_return_value1000)
{
  run_return_value(0, 0, 1000);
}
END_TEST


/*
  --------------------------------------------------------------------
  register tests from this file
  --------------------------------------------------------------------
*/

void add_08_thread2(TCase *tc, char *long_tests, char *hardcore_tests) {
  /* must run the futures test first */
  tcase_add_test(tc, test_libhpx_lco_futures);
  
  tcase_add_test(tc, test_libhpx_thread_stack_size_verify);
  tcase_add_test(tc, test_libhpx_thread_self_ptr);
  tcase_add_test(tc, test_libhpx_thread_self_ptr_ext);
  tcase_add_test(tc, test_libhpx_thread_self_ptr_sig);
  tcase_add_test(tc, test_libhpx_thread_self_ptr_ext_sig);
  //  tcase_add_test(tc, test_libhpx_thread_main_hierarchy);
  //  tcase_add_test(tc, test_libhpx_thread_main_hierarchy_ext);
  //  tcase_add_test(tc, test_libhpx_thread_main_hierarchy_sig);
  //  tcase_add_test(tc, test_libhpx_thread_main_hierarchy_ext_sig);
  tcase_add_test(tc, test_libhpx_thread_strcpy_th1_core1);
  tcase_add_test(tc, test_libhpx_thread_strcpy_th1_core1_ext);
  tcase_add_test(tc, test_libhpx_thread_strcpy_th1_core1_sig);
  tcase_add_test(tc, test_libhpx_thread_strcpy_th1_core1_ext_sig);
  tcase_add_test(tc, test_libhpx_thread_args);
  tcase_add_test(tc, test_libhpx_thread_args_ext);
  tcase_add_test(tc, test_libhpx_thread_args_sig);
  tcase_add_test(tc, test_libhpx_thread_args_ext_sig);
  tcase_add_test(tc, test_libhpx_thread_return_value1000);
  tcase_add_test(tc, test_libhpx_thread_multi_thread_set_x2);
  tcase_add_test(tc, test_libhpx_thread_multi_thread_set_x2_ext);
  tcase_add_test(tc, test_libhpx_thread_multi_thread_set_x2_sig);
  tcase_add_test(tc, test_libhpx_thread_multi_thread_set_x2_ext_sig);
  tcase_add_test(tc, test_libhpx_thread_multi_thread_set_yield1);
  tcase_add_test(tc, test_libhpx_thread_multi_thread_set_yield2);
  tcase_add_test(tc, test_libhpx_thread_multi_thread_set_yield_x2);

  if (long_tests || hardcore_tests) {
    tcase_add_test(tc, test_libhpx_thread_multi_thread_set_x32);
    tcase_add_test(tc, test_libhpx_thread_multi_thread_set_x32_ext);
    tcase_add_test(tc, test_libhpx_thread_multi_thread_set_x32_sig);
    tcase_add_test(tc, test_libhpx_thread_multi_thread_set_x32_ext_sig);
    tcase_add_test(tc, test_libhpx_thread_multi_thread_set_yield_x32);
    tcase_add_test(tc, test_libhpx_thread_multi_thread_set_yield_1core_5000);
    tcase_add_test(tc, test_libhpx_thread_multi_thread_set_yield_2core_5000);
    tcase_add_test(tc, test_libhpx_thread_multi_thread_set_yield_1024core_5000);
  }

  if (hardcore_tests) {
    tcase_add_test(tc, test_libhpx_thread_multi_thread_set_hardcore1000);
    tcase_add_test(tc, test_libhpx_thread_multi_thread_set_hardcore5000);
    tcase_add_test(tc, test_libhpx_thread_multi_thread_set_yield_hardcore1000);
    tcase_add_test(tc, test_libhpx_thread_multi_thread_set_yield_hardcore5000);
    tcase_add_test(tc, test_libhpx_thread_multi_thread_set_yield_hardcore10000);
  }
}
