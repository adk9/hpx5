
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Library Unit Test Harness - Machine Context Switching
  07_mctx.c

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


#include <math.h>
#include <stdarg.h>
#include <string.h>
#include <pthread.h>
#include "hpx.h"

#ifdef __APPLE__
  #include <mach/mach.h>
  #include <mach/mach_time.h>
#elif __linux__
  #include <time.h>
#endif


/*
 --------------------------------------------------------------------
  Global Test Data
 --------------------------------------------------------------------
*/

#ifdef __APPLE__
typedef struct {
  uint64_t begin_ts;
  uint64_t end_ts;
  uint64_t elapsed_ts;
  int64_t dev_ts;
} hpxtest_ts_t;
#elif __linux__
typedef struct {
  struct timespec begin_ts;
  struct timespec end_ts;
  uint64_t elapsed_ts;
  int64_t dev_ts;
} hpxtest_ts_t;
#endif

hpx_mctx_context_t * main_mctx;
hpx_mctx_context_t * mctx1;
hpx_mctx_context_t * mctx2;
hpx_mctx_context_t ** mctxs;
char * swap_msg;
int * context_counter;
unsigned int swap_idx;
unsigned int swap_pos;
unsigned int num_mctxs;

#ifdef __APPLE__
  /* https://developer.apple.com/library/mac/#qa/qa1398/_index.html */
  static mach_timebase_info_data_t tbi;
#endif

hpxtest_ts_t * main_ts;
hpxtest_ts_t ** mctx_ts;
uint64_t ts_elapsed;
uint64_t ts_runs;

double mean_ts;
double mode_ts;
double med_ts;
double stdev_ts;
uint64_t min_ts;
uint64_t max_ts;

char swap_const_msg1[] = "I must not fear.  Fear is the mind-killer.  Fear is the little-death that brings total obliteration. I will face my fear.  I will permit it to pass over me and through me.  And when it has gone past I will turn the inner eye to see its path.  Where the fear has gone there will be nothing... Only I will remain.";


/*
 --------------------------------------------------------------------
  TEST HELPER - Simple Register Crusher
 --------------------------------------------------------------------
*/

int register_crusher(int a, int b, char c) {
  FILE * dev_null;
  char msg[35];

  sprintf(msg, "I sure hope somebody reads this: %c", c);

  dev_null = fopen("/dev/null", "w");
  if (dev_null != NULL) {
    fwrite(msg, sizeof(char), 35, dev_null);
    fclose(dev_null);
  }

  return (a + b) + 73;
}


/*
 --------------------------------------------------------------------
  TEST HELPER - More Advanced Register Crushers
 --------------------------------------------------------------------
*/

extern volatile void _fpu_crusher(hpx_mconfig_t mcfg, uint64_t mflags);


/*
 --------------------------------------------------------------------
  TEST HELPER - Thread Seed Function
 --------------------------------------------------------------------
*/

void thread_seed(int a, int b, char c) {
  register_crusher(a, b, c);
}


/*
 --------------------------------------------------------------------
  TEST HELPER - Incrementers for hpx_mctx_makecontext()
 --------------------------------------------------------------------
*/


void increment_context_counter0(void) {
  *context_counter += 440;
}


void increment_context_counter1(int a) {
  char msg[128];

  *context_counter += a;

  sprintf(msg, "Argument 1 of 1-argument function call is incorrect (expecting 573, got %d)", a);
  ck_assert_msg(a == 573, msg);
}


void increment_context_counter2(int a, int b) {
  char msg[128];

  *context_counter += (a + b);

  sprintf(msg, "Argument 1 of 2-argument function call is incorrect (expecting 741, got %d).", a);
  ck_assert_msg(a == 741, msg);

  sprintf(msg, "Argument 2 of 2-argument function call is incorrect (expecting 123, got %d).", b);
  ck_assert_msg(b == 123, msg);
}


void increment_context_counter3(int a, int b, int c) {
  char msg[128];

  *context_counter += (a + b + c);

  sprintf(msg, "Argument 1 of 3-argument function call is incorrect (expecting 3, got %d).", a);
  ck_assert_msg(a = 3, msg);

  sprintf(msg, "Argument 2 of 3-argument function call is incorrect (expecting 1, got %d).", b);
  ck_assert_msg(b == 1, msg);

  sprintf(msg, "Argument 3 of 3-argument function call is incorrect (expecting 89, got %d).", c);
  ck_assert_msg(c == 89, msg);
}


void increment_context_counter4(int a, int b, int c, int d) {
  char msg[128];

  *context_counter += (a + b + c + d);

  sprintf(msg, "Argument 1 of 4-argument function call is incorrect (expecting 1004, got %d).", a);
  ck_assert_msg(a == 1004, msg);

  sprintf(msg, "Argument 2 of 4-argument function call is incorrect (expecting 7348, got %d).", b);
  ck_assert_msg(b == 7348, msg);

  sprintf(msg, "Argument 3 of 4-argument function call is incorrect (expecting 17, got %d).", c);
  ck_assert_msg(c == 17, msg);

  sprintf(msg, "Argument 4 of 4-argument function call is incorrect (expecting -109, got %d).", d); 
  ck_assert_msg(d == -109, msg);
}


void increment_context_counter5(int a, int b, int c, int d, int e) {
  char msg[128];
 
  *context_counter += (a + b + c + d + e);

  sprintf(msg, "Argument 1 of 5-argument function call is incorrect (expecting -870, got %d).", a);
  ck_assert_msg(a == -870, msg);

  sprintf(msg, "Argument 2 of 5-argument function call is incorrect (expecting 99999, got %d).", b);
  ck_assert_msg(b == 99999, msg);
 
  sprintf(msg, "Argument 3 of 5-argument function call is incorrect (expecting -4500, got %d).", c);
  ck_assert_msg(c == -4500, msg);

  sprintf(msg, "Argument 4 of 5-argument function call is incorrect (expecting 0, got %d).", d);
  ck_assert_msg(d == 0, msg);

  sprintf(msg, "Argument 5 of 5-argument function call is incorrect (expecting 1234540, got %d).", e);
  ck_assert_msg(e == 1234540, msg);  
}


void increment_context_counter6(int a, int b, int c, int d, int e, int f) {
  char msg[128];

  *context_counter += (a + b + c + d + e + f);

  sprintf(msg, "Argument 1 of 6-argument function call is incorrect (expecting 789, got %d).", a);
  ck_assert_msg(a == 789, msg);  

  sprintf(msg, "Argument 2 of 6-argument function call is incorrect (expecting 788, got %d).", b);
  ck_assert_msg(b == 788, msg);  

  sprintf(msg, "Argument 3 of 6-argument function call is incorrect (expecting 787, got %d).", c);
  ck_assert_msg(c == 787, msg);  

  sprintf(msg, "Argument 4 of 6-argument function call is incorrect (expecting 786, got %d).", d);
  ck_assert_msg(d == 786, msg);  

  sprintf(msg, "Argument 5 of 6-argument function call is incorrect (expecting 785, got %d).", e);
  ck_assert_msg(e == 785, msg);  

  sprintf(msg, "Argument 6 of 6-argument function call is incorrect (expecting 784, got %d).", f);
  ck_assert_msg(f == 784, msg);  
}


void increment_context_counter7(int a, int b, int c, int d, int e, int f, int g) {
  char msg[128];

  *context_counter += (a + b + c + d + e + f + g);

  sprintf(msg, "Argument 1 of 7-argument function call is incorrect (expecting 4321, got %d).", a);
  ck_assert_msg(a == 4321, msg);

  sprintf(msg, "Argument 2 of 7-argument function call is incorrect (expecting 1234, got %d).", b);
  ck_assert_msg(b == 1234, msg);

  sprintf(msg, "Argument 3 of 7-argument function call is incorrect (expecting 1324, got %d).", c);
  ck_assert_msg(c == 1324, msg);

  sprintf(msg, "Argument 4 of 7-argument function call is incorrect (expecting 3214, got %d).", d);
  ck_assert_msg(d == 3214, msg);

  sprintf(msg, "Argument 5 of 7-argument function call is incorrect (expecting 3421, got %d).", e);
  ck_assert_msg(e == 3421, msg);

  sprintf(msg, "Argument 6 of 7-argument function call is incorrect (expecting 3412, got %d).", f);
  ck_assert_msg(f == 3412, msg);

  sprintf(msg, "Argument 7 of 7-argument function call is incorrect (expecting 2143, got %d).", g);  
  ck_assert_msg(g == 2143, msg);
}


void increment_context_counter8(int a, int b, int c, int d, int e, int f, int g, int h) {
  char msg[128];

  *context_counter += (a + b + c + d + e + f + g + h);

  sprintf(msg, "Argument 1 of 8-argument function call is incorrect (expecting 7, got %d).", a);  
  ck_assert_msg(a == 7, msg);

  sprintf(msg, "Argument 2 of 8-argument function call is incorrect (expecting 8, got %d).", b);  
  ck_assert_msg(b == 8, msg);

  sprintf(msg, "Argument 3 of 8-argument function call is incorrect (expecting 78, got %d).", c);  
  ck_assert_msg(c == 78, msg);

  sprintf(msg, "Argument 4 of 8-argument function call is incorrect (expecting 87, got %d).", d);  
  ck_assert_msg(d == 87, msg);

  sprintf(msg, "Argument 5 of 8-argument function call is incorrect (expecting 778, got %d).", e);  
  ck_assert_msg(e == 778, msg);

  sprintf(msg, "Argument 6 of 8-argument function call is incorrect (expecting 887, got %d).", f);  
  ck_assert_msg(f == 887, msg);

  sprintf(msg, "Argument 7 of 8-argument function call is incorrect (expecting 787, got %d).", g);  
  ck_assert_msg(g == 787, msg);

  sprintf(msg, "Argument 8 of 8-argument function call is incorrect (expecting 878, got %d).", h);  
  ck_assert_msg(h == 878, msg);
}


/*
 --------------------------------------------------------------------
  TEST HELPER - Test Harness for hpx_mctx_getcontext()
 --------------------------------------------------------------------
*/

void run_getcontext(uint64_t mflags) {
  hpx_mctx_context_t mctx;
  hpx_context_t * ctx;
  hpx_xmmreg_t * xmmreg;
  hpx_config_t cfg;
  long double * x87reg;
  long double x87_epsilon = 0.000000000000001;
  long double x87_abs;
  uint32_t xmmvals[4];
  char msg[256];
  sigset_t sigs_old;
  sigset_t sigs;

  /* initialize our configuration */
  hpx_config_init(&cfg);
  hpx_config_set_switch_flags(&cfg, mflags);

  /* get a thread context */
  ctx = hpx_ctx_create(&cfg);
  ck_assert_msg(ctx != NULL, "Could not get a thread context.");

  memset(&mctx, 0, sizeof(hpx_mctx_context_t));

  /* crush that mean old FPU */
  (void) _fpu_crusher(ctx->mcfg, mflags);

  /* set some signals in the thread signal mask (if we care about that) */
  if (mflags & HPX_MCTX_SWITCH_SIGNALS) {
    sigemptyset(&sigs);
    sigaddset(&sigs, SIGTERM);
    sigaddset(&sigs, SIGABRT);
    sigaddset(&sigs, SIGPIPE);
    pthread_sigmask(SIG_SETMASK, &sigs, &sigs_old);
  }
  
  /* make sure we can call getcontext without a problem */
  hpx_mctx_getcontext(&mctx, ctx->mcfg, mflags);

  /* reset the signal mask (if we care) */
  if (mflags & HPX_MCTX_SWITCH_SIGNALS) {
    pthread_sigmask(SIG_SETMASK, &sigs_old, 0);
  }

#ifdef __x86_64__
  /* test for some easy stuff */
  ck_assert_msg(mctx.regs.rsp != 0, "Stack pointer was not saved.");
  ck_assert_msg(mctx.regs.rip != 0, "Instruction pointer was not saved.");

  /* test function call registers */
  sprintf(msg, "First argument passing register (RDI) was not saved (expected %ld, got %ld).", (uint64_t) &mctx, mctx.regs.rdi);
  ck_assert_msg(mctx.regs.rdi == (uint64_t) &mctx, msg);

  sprintf(msg, "Second argument passing register (RSI) was not saved (expected %ld, got %ld).", (uint64_t) ctx->mcfg, mctx.regs.rsi);
  ck_assert_msg(mctx.regs.rsi == (uint64_t) ctx->mcfg, msg);

  sprintf(msg, "Third argument passing register (RDX) was not saved (expected %ld, got %ld).", mflags, mctx.regs.rdx);
  ck_assert_msg(mctx.regs.rdx == mflags, msg);

  /* test crushed FPU */
  if ((mflags & HPX_MCTX_SWITCH_EXTENDED) && (ctx->mcfg & _HPX_MCONFIG_HAS_FPU)) {
    /* ST(0) should have a +1.00 in it */
    x87reg = (long double *) &mctx.regs.fpregs.sts[0];
    sprintf(msg, "FPU register ST(0) was not saved (expected 1.00, got %.2Lf).", (long double) *x87reg);
    ck_assert_msg(*x87reg == 1.00, msg);

    /* ST(1) should have a value of +0.00 (as opposed to NaN) */
    x87reg = (long double *) &mctx.regs.fpregs.sts[1];
    sprintf(msg, "FPU register ST(1) was not saved (expected 0.00, got %.2Lf).", (long double) *x87reg);
    ck_assert_msg((long double) *x87reg == 0.00, msg);

    /* ST(2) should have a value of log2(e) to at least 15 decimal places */
    x87reg = (long double *) &mctx.regs.fpregs.sts[2];
    x87_abs = fabs((long double) *x87reg - M_LOG2E);
    sprintf(msg, "FPU register ST(2) was not saved (expected %.19Lf, got %.19Lf).", (long double) M_LOG2E, (long double) *x87reg);
    ck_assert_msg((x87_abs < x87_epsilon), msg);

    /* ST(3) should have a value of +1.00 */
    x87reg = (long double *) &mctx.regs.fpregs.sts[3];
    sprintf(msg, "FPU register ST(3) was not saved (expected 1.00, got %.2Lf).", (long double) *x87reg);
    ck_assert_msg(*x87reg == 1.00, msg);

    /* ST(4) should have a value of Pi to at least 15 decimal places */
    x87reg = (long double *) &mctx.regs.fpregs.sts[4];
    x87_abs = fabs((long double) *x87reg - M_PI);
    sprintf(msg, "FPU register ST(4) was not saved (expected %.19Lf, got %.19Lf).", (long double) M_PI, (long double) *x87reg);
    ck_assert_msg((x87_abs < x87_epsilon), msg);

    /* the abridged tag word should indicate valid values for ST(0)-ST(4) */
    /* FTW = 11111000b = 0xF8 */
    sprintf(msg, "Abridged FPU tag word was not saved (expected 0xF8, got %02x).", mctx.regs.fpregs.ftw);
    ck_assert_msg(mctx.regs.fpregs.ftw == 0xF8, msg);

    /* we would check our last non-control fopcode here but that might be turned off and we */
    /* can't check it unless we're in Ring 0.  that is, we're screwed. */
  }

  /* test our signal set */
  if (mflags & HPX_MCTX_SWITCH_SIGNALS) {
    ck_assert_msg(sigismember(&mctx.sigs, SIGTERM) == 1, "Signals were not saved (expected SIGTERM).");
    ck_assert_msg(sigismember(&mctx.sigs, SIGABRT) == 1, "Signals were not saved (expected SIGABRT).");
    ck_assert_msg(sigismember(&mctx.sigs, SIGPIPE) == 1, "Signals were not saved (expected SIGPIPE).");
  }
#endif
  /* clean up */
  hpx_ctx_destroy(ctx);
}


/*
 --------------------------------------------------------------------
  TEST HELPER - Test Runner for hpx_mctx_setcontext()
 --------------------------------------------------------------------
*/

void run_setcontext_counter(uint64_t mflags) {
  hpx_mctx_context_t mctx;
  hpx_context_t * ctx;
  hpx_config_t cfg;
  char msg[128];

  /* init our config */
  hpx_config_init(&cfg);
  hpx_config_set_switch_flags(&cfg, mflags);

  /* get a thread context */
  ctx = hpx_ctx_create(&cfg);
  ck_assert_msg(ctx != NULL, "Could not get a thread context.");

  context_counter = (int *) malloc(sizeof(int));
  if (context_counter != NULL) {
    *context_counter = 0;
  }

  ck_assert_msg(context_counter != NULL, "Could not allocate a counter to test context switching.");

  memset(&mctx, 0, sizeof(hpx_mctx_context_t));
 
#ifdef __x86_64__
  hpx_mctx_getcontext(&mctx, ctx->mcfg, mflags);

  *context_counter += 1;

  /* do something that (hopefully) changes the value of our registers */
  register_crusher(4,92, 'z'); 

  if (*context_counter < 100) {
    hpx_mctx_setcontext(&mctx, ctx->mcfg, mflags);
  } 

  ck_assert_msg(*context_counter == 100, "Test counter was not incremented in context switch.");
#endif

  hpx_free(context_counter);
  hpx_ctx_destroy(ctx);
}


/*
 --------------------------------------------------------------------
  TEST HELPER - Test Runner for hpx_mctx_makecontext()
 --------------------------------------------------------------------
*/

void run_makecontext_counter(uint64_t mflags, int mk_limit, void * func, int argc, ...) {
  hpx_context_t * ctx;
  hpx_config_t cfg;
  char st1[8192] __attribute__((aligned (16)));
  char msg[128];
  va_list argv;

  /* init our config */
  hpx_config_init(&cfg);
  hpx_config_set_switch_flags(&cfg, mflags);

  /* allocate machine contexts */
  mctx1 = (hpx_mctx_context_t *) hpx_alloc(sizeof(hpx_mctx_context_t));
  ck_assert_msg(mctx1 != NULL, "Could not allocate machine context 1.");

  mctx2 = (hpx_mctx_context_t *) hpx_alloc(sizeof(hpx_mctx_context_t));
  ck_assert_msg(mctx2 != NULL, "Could not allocate machine context 2.");

  /* get a thread context */
  ctx = hpx_ctx_create(&cfg);
  ck_assert_msg(ctx != NULL, "Could not get a thread context.");

  /* allocate our test counter */ 
  context_counter = (int *) malloc(sizeof(int));
  if (context_counter != NULL) {
    *context_counter = 0;
  }

  ck_assert_msg(context_counter != NULL, "Could not allocate a counter to test context switching.");
 
  /* initialize machine contexts */
  memset(mctx1, 0, sizeof(hpx_mctx_context_t));
  memset(mctx2, 0, sizeof(hpx_mctx_context_t));
  
  /* save the current context */
  hpx_mctx_getcontext(mctx1, ctx->mcfg, mflags);  

  /* start parsing variadic arguments */
  va_start(argv, argc);

  /* initialize a new context */
  hpx_mctx_makecontext_va(mctx2, mctx1, st1, sizeof(st1), ctx->mcfg, mflags, func, argc, &argv);
  va_end(argv);

  /* crush some registers */
  register_crusher(4, 92, 'z');
 
  /* keep switching back into a new context until our counter is updated */
  if (*context_counter < mk_limit) {
    hpx_mctx_setcontext(mctx2, ctx->mcfg, mk_limit);
  } 
  
  ck_assert_msg(*context_counter == mk_limit, "Test counter has incorrect value after context switch.");

  /* clean up */
  free(context_counter);
  hpx_ctx_destroy(ctx);

  hpx_free(mctx2);
  hpx_free(mctx1);
}


/*
 --------------------------------------------------------------------
  TEST HELPER - Worker Function for run_swapcontext_copy_chain().
 --------------------------------------------------------------------
*/

void swapcontext_copy_chain_worker(hpx_mconfig_t mcfg, uint64_t mflags, char * msg, unsigned int msg_len) {
  hpx_mctx_context_t * my_mctx = mctxs[swap_idx];
  hpx_mctx_context_t * next_mctx;

  while (swap_pos < msg_len) {
    swap_msg[swap_pos] = msg[swap_pos];
    swap_pos += 1;
   
    swap_msg[swap_pos] = '\0';

    swap_idx = (++swap_idx % num_mctxs);
    next_mctx = mctxs[swap_idx];

    hpx_mctx_swapcontext(my_mctx, next_mctx, mcfg, mflags);
  }
}


/*
 --------------------------------------------------------------------
  TEST HELPER - Test Runner for hpx_mctx_swapcontext that copies
  a string in by daisy-chaining context switches between a set of
  machine contexts created by hpx_mctx_makecontext().
 --------------------------------------------------------------------
*/

void run_swapcontext_copy_chain(uint64_t mflags, unsigned int num_mctx, char * orig_msg, unsigned int orig_len) {
  hpx_mctx_context_t * mctx;
  hpx_context_t * ctx;
  hpx_config_t cfg;
  unsigned int idx;
  char msg[128 + orig_len + 1];  // yeah, I know this is sooper secure
  char * stk;

  /* init our config */
  hpx_config_init(&cfg);
  hpx_config_set_switch_flags(&cfg, mflags);

  /* get a thread context */
  ctx = hpx_ctx_create(&cfg);
  ck_assert_msg(ctx != NULL, "Could not get a thread context.");

  /* create the main machine context */
  main_mctx = (hpx_mctx_context_t *) hpx_alloc(sizeof(hpx_mctx_context_t));
  ck_assert_msg(main_mctx != NULL, "Could not allocate a main machine context.");

  /* create child machine contexts */
  mctxs = (hpx_mctx_context_t **) hpx_alloc(sizeof(hpx_mctx_context_t *) * num_mctx);
  ck_assert_msg(mctxs != NULL, "Could not create machine child contexts.");

  /* initialize our other stuff */
  swap_msg = (char *) hpx_alloc(sizeof(char) * (orig_len + 1));
  ck_assert_msg(swap_msg != NULL, "Could not allocate a context swap message buffer.");

  swap_msg[0] = '\0';
  swap_idx = 0;
  swap_pos = 0;
  num_mctxs = num_mctx;

  /* get our main context */
  hpx_mctx_getcontext(main_mctx, ctx->mcfg, mflags);

  /* initialize our contexts */
  if (swap_pos == 0) {
    for (idx = 0; idx < num_mctx; idx++) {
      mctx = (hpx_mctx_context_t *) hpx_alloc(sizeof(hpx_mctx_context_t));
      ck_assert_msg(mctx != NULL, "Could not allocate machine child context %d.", idx);

      stk = (void *) hpx_alloc(256);
      sprintf(msg, "Could not allocate stack (256 bytes) for machine context %d.", idx);
      ck_assert_msg(stk != NULL, msg);
  
      hpx_mctx_makecontext(mctx, main_mctx, stk, 256, ctx->mcfg, mflags, swapcontext_copy_chain_worker, 4, ctx->mcfg, mflags, orig_msg, orig_len);
   
      mctxs[idx] = mctx;
    }
  }

  hpx_mctx_swapcontext(main_mctx, mctxs[0], ctx->mcfg, mflags);

  sprintf(msg, "Index was not incremented during context swap (expected %d, got %d).", orig_len, swap_pos);
  ck_assert_msg(swap_pos == orig_len, msg);

  sprintf(msg, "String was not copied (got: \"%s\")", swap_msg);
  ck_assert_msg((strcmp(swap_msg, orig_msg) == 0), msg);

  /* clean up */
  hpx_free(swap_msg);

  for (idx = 0; idx < num_mctxs; idx++) {
    mctx = mctxs[idx];
    hpx_free(mctx);
  }

  hpx_free(mctxs);
  hpx_free(main_mctx);
  hpx_ctx_destroy(ctx);
}


/*
 --------------------------------------------------------------------
  TEST HELPER - Worker Function for run_swapcontext_memset_star().
 --------------------------------------------------------------------
*/

void swapcontext_memset_star_worker(hpx_mconfig_t mcfg, uint64_t mflags, unsigned int start_idx, unsigned int end_idx) {
  hpx_mctx_context_t * my_mctx;
  unsigned int cur_idx = start_idx;

  while (cur_idx < end_idx) {
    my_mctx = mctxs[swap_idx];

    swap_msg[cur_idx] = 73;
    cur_idx++;

    if (cur_idx > swap_pos) {
      swap_pos = cur_idx;
    }

    hpx_mctx_swapcontext(my_mctx, main_mctx, mcfg, mflags);
  }
}


/*
 --------------------------------------------------------------------
  TEST HELPER - Test Runner for hpx_mctx_swapcontext() that does
  a memset()-like operation on a preallocated buffer.  Each 
  coroutine (swapcontext_memset_star_worker) takes a 1K block of
  bytes in the buffer and sets one byte on each iteration.  It then
  performs a context switch back into the main context so another
  coroutine can be selected.  This continues until the buffer is
  set, and then the main thread verifies each byte was set
  correctly by the coroutines.
 --------------------------------------------------------------------
*/

void run_swapcontext_memset_star(uint64_t mflags, unsigned int num_mctxs) {
  hpx_mctx_context_t * mctx;
  hpx_context_t * ctx;
  hpx_config_t cfg;
  unsigned int start_idx;
  unsigned int idx;
  char msg[128];
  void * stk;

  /* init our config */
  hpx_config_init(&cfg);
  hpx_config_set_switch_flags(&cfg, mflags);

  /* get a thread context */
  ctx = hpx_ctx_create(&cfg);
  ck_assert_msg(ctx != NULL, "Could not get a thread context.");

  /* create a main machine context */
  main_mctx = (hpx_mctx_context_t *) hpx_alloc(sizeof(hpx_mctx_context_t));
  ck_assert_msg(main_mctx != NULL, "Could not allocate a main machine context.");

  /* save our current machine context */
  memset(main_mctx, 0, sizeof(hpx_mctx_context_t));
  hpx_mctx_getcontext(main_mctx, ctx->mcfg, mflags);

  /* create our swap buffer (20MB) */
  swap_msg = (char *) hpx_alloc(sizeof(char) * (num_mctxs * 1024));
  sprintf(msg, "Could not allocate swap buffer (%d KB).", num_mctxs);
  ck_assert_msg(swap_msg != NULL, msg);

  /* create & initialize our child machine contexts */
  mctxs = (hpx_mctx_context_t **) hpx_alloc(sizeof(hpx_mctx_context_t *) * num_mctxs);
  ck_assert_msg(mctxs != NULL, "Could not allocate child machine context pointers.");

  start_idx = 0;
  swap_pos = 0;
  swap_idx = 0;

  for (idx = 0; idx < num_mctxs; idx++) {
    mctx = (hpx_mctx_context_t *) hpx_alloc(sizeof(hpx_mctx_context_t));
    sprintf(msg, "Could not allocate child machine context %d.", idx);
    ck_assert_msg(mctx != NULL, msg);

    stk = (void *) hpx_alloc(1024);
    sprintf(msg, "Could not allocate a stack for child machine context %d.", idx);
    ck_assert_msg(stk != NULL, msg);

    hpx_mctx_makecontext(mctx, main_mctx, stk, 1024, ctx->mcfg, mflags, swapcontext_memset_star_worker, 4, ctx->mcfg, mflags, start_idx, (start_idx + 1024));
    start_idx += 1024;    

    mctxs[idx] = mctx;
  }

  while (swap_pos < (num_mctxs * 1024)) {
    hpx_mctx_swapcontext(main_mctx, mctxs[swap_idx], ctx->mcfg, mflags);
    swap_idx = (++swap_idx % num_mctxs);
  }

  sprintf(msg, "Index was not incremented during context swap (expected %d, got %d).", (num_mctxs * 1024), swap_pos);
  ck_assert_msg(swap_pos == (num_mctxs * 1024), msg);

  for (idx = 0; idx < (num_mctxs * 1024); idx++) {
    sprintf(msg, "Swap buffer is incorrect at position %d (expected 73, got %d).", idx, (int) swap_msg[idx]);
    ck_assert_msg(swap_msg[idx] == 73, msg);
  }

  /* clean up */
  hpx_free(swap_msg);

  for (idx = 0; idx < num_mctxs; idx++) {
    mctx = mctxs[idx];
    hpx_free(mctx->sp);
    hpx_free(mctx);
  }  

  hpx_free(mctxs);
  hpx_free(main_mctx);
  hpx_ctx_destroy(ctx);
}


/*
 --------------------------------------------------------------------
  TEST HELPER: Test Runner for hpx_mctx_swapcontext() timings.
 --------------------------------------------------------------------
*/



/*
 ====================================================================
  BEGIN TESTS
 ====================================================================
*/

/*
 --------------------------------------------------------------------
  TEST: saving a context without any flags
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_getcontext)
{
  run_getcontext(0);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: saving a context with extended (FPU) state
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_getcontext_ext)
{
  run_getcontext(HPX_MCTX_SWITCH_EXTENDED);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: saving a context with signals
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_getcontext_sig)
{
  run_getcontext(HPX_MCTX_SWITCH_SIGNALS);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: saving a context with extended (FPU) state and signals
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_getcontext_ext_sig)
{
  run_getcontext(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: context switching without saving extended (FPU) state
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_setcontext)
{
  run_setcontext_counter(0);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: context switching, saving extended (FPU) state
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_setcontext_ext)
{
  run_setcontext_counter(HPX_MCTX_SWITCH_EXTENDED);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: context switching, saving signals
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_setcontext_sig)
{
  run_setcontext_counter(HPX_MCTX_SWITCH_SIGNALS);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: context switching, saving extended (FPU) state and signals
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_setcontext_ext_sig)
{
  run_setcontext_counter(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: hpx_mctx_makecontext with no arguments

  We need to test this with a number of different arguments
  because they're passed in different ways depending on how many
  there are, as per the x86_64 ABI.

  At a bare minimum, we need to test with 0, 1, 6, 7, and >7 
  arguments.
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_makecontext_0arg)
{
  run_makecontext_counter(0, 44000, increment_context_counter0, 0);
}
END_TEST


START_TEST (test_libhpx_mctx_makecontext_0arg_ext)
{
  run_makecontext_counter(HPX_MCTX_SWITCH_EXTENDED, 44000, increment_context_counter0, 0);
}
END_TEST


START_TEST (test_libhpx_mctx_makecontext_0arg_sig)
{
  run_makecontext_counter(HPX_MCTX_SWITCH_SIGNALS, 44000, increment_context_counter0, 0);
}
END_TEST


START_TEST (test_libhpx_mctx_makecontext_0arg_ext_sig)
{
  run_makecontext_counter(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, 44000, increment_context_counter0, 0);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: hpx_mctx_makecontext with one argument
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_makecontext_1arg)
{
  run_makecontext_counter(0, 57300, increment_context_counter1, 1, 573);
}
END_TEST


START_TEST (test_libhpx_mctx_makecontext_1arg_ext)
{
  run_makecontext_counter(HPX_MCTX_SWITCH_EXTENDED, 57300, increment_context_counter1, 1, 573);
}
END_TEST


START_TEST (test_libhpx_mctx_makecontext_1arg_sig)
{
  run_makecontext_counter(HPX_MCTX_SWITCH_SIGNALS, 57300, increment_context_counter1, 1, 573);
}
END_TEST


START_TEST (test_libhpx_mctx_makecontext_1arg_ext_sig)
{
  run_makecontext_counter(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, 57300, increment_context_counter1, 1, 573);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: hpx_mctx_makecontext with two arguments
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_makecontext_2arg)
{
  run_makecontext_counter(0, 86400, increment_context_counter2, 2, 741, 123);
}
END_TEST

START_TEST (test_libhpx_mctx_makecontext_2arg_ext)
{
  run_makecontext_counter(HPX_MCTX_SWITCH_EXTENDED, 86400, increment_context_counter2, 2, 741, 123);
}
END_TEST


START_TEST (test_libhpx_mctx_makecontext_2arg_sig)
{
  run_makecontext_counter(HPX_MCTX_SWITCH_SIGNALS, 86400, increment_context_counter2, 2, 741, 123);
}
END_TEST


START_TEST (test_libhpx_mctx_makecontext_2arg_ext_sig)
{
  run_makecontext_counter(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, 86400, increment_context_counter2, 2, 741, 123);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: hpx_mctx_makecontext with three arguments
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_makecontext_3arg)
{
  run_makecontext_counter(0, 9300, increment_context_counter3, 3, 3, 1, 89);
}
END_TEST


START_TEST (test_libhpx_mctx_makecontext_3arg_ext)
{
  run_makecontext_counter(HPX_MCTX_SWITCH_EXTENDED, 9300, increment_context_counter3, 3, 3, 1, 89);
}
END_TEST


START_TEST (test_libhpx_mctx_makecontext_3arg_sig)
{
  run_makecontext_counter(HPX_MCTX_SWITCH_SIGNALS, 9300, increment_context_counter3, 3, 3, 1, 89);
}
END_TEST


START_TEST (test_libhpx_mctx_makecontext_3arg_ext_sig)
{
  run_makecontext_counter(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, 9300, increment_context_counter3, 3, 3, 1, 89);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: hpx_mctx_makecontext with four arguments
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_makecontext_4arg)
{
  run_makecontext_counter(0, 826000, increment_context_counter4, 4, 1004, 7348, 17, -109);
}
END_TEST


START_TEST (test_libhpx_mctx_makecontext_4arg_ext)
{
  run_makecontext_counter(HPX_MCTX_SWITCH_EXTENDED, 826000, increment_context_counter4, 4, 1004, 7348, 17, -109);
}
END_TEST


START_TEST (test_libhpx_mctx_makecontext_4arg_sig)
{
  run_makecontext_counter(HPX_MCTX_SWITCH_SIGNALS, 826000, increment_context_counter4, 4, 1004, 7348, 17, -109);
}
END_TEST


START_TEST (test_libhpx_mctx_makecontext_4arg_ext_sig)
{
  run_makecontext_counter(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, 826000, increment_context_counter4, 4, 1004, 7348, 17, -109);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: hpx_mctx_makecontext with five arguments
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_makecontext_5arg)
{
  run_makecontext_counter(0, 132916900, increment_context_counter5, 5, -870, 99999, -4500, 0, 1234540);
}
END_TEST


START_TEST (test_libhpx_mctx_makecontext_5arg_ext)
{
  run_makecontext_counter(HPX_MCTX_SWITCH_EXTENDED, 132916900, increment_context_counter5, 5, -870, 99999, -4500, 0, 1234540);
}
END_TEST


START_TEST (test_libhpx_mctx_makecontext_5arg_sig)
{
  run_makecontext_counter(HPX_MCTX_SWITCH_SIGNALS, 132916900, increment_context_counter5, 5, -870, 99999, -4500, 0, 1234540);
}
END_TEST


START_TEST (test_libhpx_mctx_makecontext_5arg_ext_sig)
{
  run_makecontext_counter(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, 132916900, increment_context_counter5, 5, -870, 99999, -4500, 0, 1234540);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: hpx_mctx_makecontext with six arguments
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_makecontext_6arg)
{
  run_makecontext_counter(0, 471900, increment_context_counter6, 6, 789, 788, 787, 786, 785, 784);
}
END_TEST


START_TEST (test_libhpx_mctx_makecontext_6arg_ext)
{
  run_makecontext_counter(HPX_MCTX_SWITCH_EXTENDED, 471900, increment_context_counter6, 6, 789, 788, 787, 786, 785, 784);
}
END_TEST


START_TEST (test_libhpx_mctx_makecontext_6arg_sig)
{
  run_makecontext_counter(HPX_MCTX_SWITCH_SIGNALS, 471900, increment_context_counter6, 6, 789, 788, 787, 786, 785, 784);
}
END_TEST


START_TEST (test_libhpx_mctx_makecontext_6arg_ext_sig)
{
  run_makecontext_counter(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, 471900, increment_context_counter6, 6, 789, 788, 787, 786, 785, 784);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: hpx_mctx_makecontext with seven arguments
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_makecontext_7arg)
{
  run_makecontext_counter(0, 1906900, increment_context_counter7, 7, 4321, 1234, 1324, 3214, 3421, 3412, 2143);
}
END_TEST


START_TEST (test_libhpx_mctx_makecontext_7arg_ext)
{
  run_makecontext_counter(HPX_MCTX_SWITCH_EXTENDED, 1906900, increment_context_counter7, 7, 4321, 1234, 1324, 3214, 3421, 3412, 2143);
}
END_TEST


START_TEST (test_libhpx_mctx_makecontext_7arg_sig)
{
  run_makecontext_counter(HPX_MCTX_SWITCH_SIGNALS, 1906900, increment_context_counter7, 7, 4321, 1234, 1324, 3214, 3421, 3412, 2143);
}
END_TEST


START_TEST (test_libhpx_mctx_makecontext_7arg_ext_sig)
{
  run_makecontext_counter(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, 1906900, increment_context_counter7, 7, 4321, 1234, 1324, 3214, 3421, 3412, 2143);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: hpx_mctx_makecontext with eight arguments
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_makecontext_8arg)
{
  run_makecontext_counter(0, 351000, increment_context_counter8, 8, 7, 8, 78, 87, 778, 887, 787, 878);
}
END_TEST


START_TEST (test_libhpx_mctx_makecontext_8arg_ext)
{
  run_makecontext_counter(HPX_MCTX_SWITCH_EXTENDED, 351000, increment_context_counter8, 8, 7, 8, 78, 87, 778, 887, 787, 878);
}
END_TEST


START_TEST (test_libhpx_mctx_makecontext_8arg_sig)
{
  run_makecontext_counter(HPX_MCTX_SWITCH_SIGNALS, 351000, increment_context_counter8, 8, 7, 8, 78, 87, 778, 887, 787, 878);
}
END_TEST


START_TEST (test_libhpx_mctx_makecontext_8arg_ext_sig)
{
  run_makecontext_counter(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, 351000, increment_context_counter8, 8, 7, 8, 78, 87, 778, 887, 787, 878);
}
END_TEST



/*
 --------------------------------------------------------------------
  TEST: hpx_mctx_swapcontext with no flags and one machine context
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_swapcontext_chain1)
{
  run_swapcontext_copy_chain(0, 1, swap_const_msg1, strlen(swap_const_msg1));
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: hpx_mctx_swapcontext with no flags and two machine contexts
  switchin in a chain
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_swapcontext_chain2)
{
  run_swapcontext_copy_chain(0, 2, swap_const_msg1, strlen(swap_const_msg1));
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: hpx_mctx_swapcontext with no flags and 310 machine contexts
  switching in a chain

  (310 machine contexts is the length of the string - 1)
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_swapcontext_chain310)
{
  run_swapcontext_copy_chain(0, 310, swap_const_msg1, strlen(swap_const_msg1));
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: hpx_mctx_swapcontext with no flags and 311 machine contexts
  switching in a chain

  (311 is the length of the string, or one context per character)
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_swapcontext_chain311)
{
  run_swapcontext_copy_chain(0, 311, swap_const_msg1, strlen(swap_const_msg1));
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: hpx_mctx_swapcontext with no flags and 312 machine contexts
  switching in a chain

  (312 is the length of the string + 1)
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_swapcontext_chain312)
{
  run_swapcontext_copy_chain(0, 312, swap_const_msg1, strlen(swap_const_msg1));
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: hpx_mctx_swapcontext with no flags and 8000 machine
  contexts switching in a chain
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_swapcontext_chain8000)
{
  run_swapcontext_copy_chain(0, 8000, swap_const_msg1, strlen(swap_const_msg1));
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: hpx_mctx_swapcontext with no flags and 90000 machine
  contexts switching in a chain
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_swapcontext_chain90000)
{
  run_swapcontext_copy_chain(0, 90000, swap_const_msg1, strlen(swap_const_msg1));
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: hpx_mctx_swapcontext with no flags and one machine context
  switching in a star topology
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_swapcontext_star1)
{
  run_swapcontext_memset_star(0, 1);

  //  printf("test_libhpx_mctx_swapcontext_star1,%ld,%.1f,%ld,%ld,%.1f\n", ts_runs, (double) mean_ts, max_ts, min_ts, stdev_ts);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: hpx_mctx_swapcontext with no flags and 2 machine contexts
  switching in a star topology
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_swapcontext_star2)
{
  run_swapcontext_memset_star(0, 2);

  //  printf("test_libhpx_mctx_swapcontext_star2,%ld,%.1f,%ld,%ld,%.1f\n", ts_runs, (double) mean_ts, max_ts, min_ts, stdev_ts);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: hpx_mctx_swapcontext with no flags and 10 machine contexts
  switching in a star topology
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_swapcontext_star10)
{
  run_swapcontext_memset_star(0, 10);

  //  printf("test_libhpx_mctx_swapcontext_star10,%ld,%.1f,%ld,%ld,%.1f\n", ts_runs, (double) mean_ts, max_ts, min_ts, stdev_ts);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: hpx_mctx_swapcontext with no flags and 1,000 machine 
  contexts switching in a star topology
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_swapcontext_star1000)
{
  run_swapcontext_memset_star(0, 1000);

  //  printf("test_libhpx_mctx_swapcontext_star1000,%ld,%.1f,%ld,%ld,%.1f\n", ts_runs, (double) mean_ts, max_ts, min_ts, stdev_ts);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: hpx_mctx_swapcontext with no flags and 5,000 machine 
  contexts switching in a star topology
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_swapcontext_star5000)
{
  run_swapcontext_memset_star(0, 5000);
  
  //  printf("test_libhpx_mctx_swapcontext_star5000,%ld,%.1f,%ld,%ld,%.1f\n", ts_runs, (double) mean_ts, max_ts, min_ts, stdev_ts);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: hpx_mctx_swapcontext with no flags and one machine context
  switching in a star topology, saving extended state
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_swapcontext_star1_ext)
{
  run_swapcontext_memset_star(HPX_MCTX_SWITCH_EXTENDED, 1);

  //  printf("test_libhpx_mctx_swapcontext_star1_ext,%ld,%.1f,%ld,%ld,%.1f\n", ts_runs, (double) mean_ts, max_ts, min_ts, stdev_ts);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: hpx_mctx_swapcontext with no flags and 2 machine contexts
  switching in a star topology, saving extended state
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_swapcontext_star2_ext)
{
  run_swapcontext_memset_star(HPX_MCTX_SWITCH_EXTENDED, 2);

  //  printf("test_libhpx_mctx_swapcontext_star2_ext,%ld,%.1f,%ld,%ld,%.1f\n", ts_runs, (double) mean_ts, max_ts, min_ts, stdev_ts);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: hpx_mctx_swapcontext with no flags and 10 machine contexts
  switching in a star topology, saving extended state
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_swapcontext_star10_ext)
{
  run_swapcontext_memset_star(HPX_MCTX_SWITCH_EXTENDED, 10);

  //  printf("test_libhpx_mctx_swapcontext_star10_ext,%ld,%.1f,%ld,%ld,%.1f\n", ts_runs, (double) mean_ts, max_ts, min_ts, stdev_ts);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: hpx_mctx_swapcontext with no flags and 1,000 machine 
  contexts switching in a star topology, saving extended state
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_swapcontext_star1000_ext)
{
  run_swapcontext_memset_star(HPX_MCTX_SWITCH_EXTENDED, 1000);

  //  printf("test_libhpx_mctx_swapcontext_star1000_ext,%ld,%.1f,%ld,%ld,%.1f\n", ts_runs, (double) mean_ts, max_ts, min_ts, stdev_ts);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: hpx_mctx_swapcontext with no flags and 5,000 machine 
  contexts switching in a star topology, saving extended state
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_swapcontext_star5000_ext)
{
  run_swapcontext_memset_star(HPX_MCTX_SWITCH_EXTENDED, 5000);
  
  //  printf("test_libhpx_mctx_swapcontext_star5000_ext,%ld,%.1f,%ld,%ld,%.1f\n", ts_runs, (double) mean_ts, max_ts, min_ts, stdev_ts);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: hpx_mctx_swapcontext with no flags and one machine context
  switching in a star topology, saving signals
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_swapcontext_star1_sig)
{
  run_swapcontext_memset_star(HPX_MCTX_SWITCH_SIGNALS, 1);

  //  printf("test_libhpx_mctx_swapcontext_star1_sig,%ld,%.1f,%ld,%ld,%.1f\n", ts_runs, (double) mean_ts, max_ts, min_ts, stdev_ts);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: hpx_mctx_swapcontext with no flags and 2 machine contexts
  switching in a star topology, saving signals
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_swapcontext_star2_sig)
{
  run_swapcontext_memset_star(HPX_MCTX_SWITCH_SIGNALS, 2);

  //  printf("test_libhpx_mctx_swapcontext_star2_sig,%ld,%.1f,%ld,%ld,%.1f\n", ts_runs, (double) mean_ts, max_ts, min_ts, stdev_ts);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: hpx_mctx_swapcontext with no flags and 10 machine contexts
  switching in a star topology, saving signals
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_swapcontext_star10_sig)
{
  run_swapcontext_memset_star(HPX_MCTX_SWITCH_SIGNALS, 10);

  //  printf("test_libhpx_mctx_swapcontext_star10_sig,%ld,%.1f,%ld,%ld,%.1f\n", ts_runs, (double) mean_ts, max_ts, min_ts, stdev_ts);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: hpx_mctx_swapcontext with no flags and 1,000 machine 
  contexts switching in a star topology, saving signals
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_swapcontext_star1000_sig)
{
  run_swapcontext_memset_star(HPX_MCTX_SWITCH_SIGNALS, 1000);

  //  printf("test_libhpx_mctx_swapcontext_star1000_sig,%ld,%.1f,%ld,%ld,%.1f\n", ts_runs, (double) mean_ts, max_ts, min_ts, stdev_ts);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: hpx_mctx_swapcontext with no flags and 5,000 machine 
  contexts switching in a star topology, saving signals
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_swapcontext_star5000_sig)
{
  run_swapcontext_memset_star(HPX_MCTX_SWITCH_SIGNALS, 5000);
  
  //  printf("test_libhpx_mctx_swapcontext_star5000_sig,%ld,%.1f,%ld,%ld,%.1f\n", ts_runs, (double) mean_ts, max_ts, min_ts, stdev_ts);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: hpx_mctx_swapcontext with no flags and one machine context
  switching in a star topology, saving extended state and signals
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_swapcontext_star1_ext_sig)
{
  run_swapcontext_memset_star(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, 1);

  //  printf("test_libhpx_mctx_swapcontext_star1_ext_sig,%ld,%.1f,%ld,%ld,%.1f\n", ts_runs, (double) mean_ts, max_ts, min_ts, stdev_ts);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: hpx_mctx_swapcontext with no flags and 2 machine contexts
  switching in a star topology, saving extended state and signals
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_swapcontext_star2_ext_sig)
{
  run_swapcontext_memset_star(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, 2);

  //  printf("test_libhpx_mctx_swapcontext_star2_ext_sig,%ld,%.1f,%ld,%ld,%.1f\n", ts_runs, (double) mean_ts, max_ts, min_ts, stdev_ts);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: hpx_mctx_swapcontext with no flags and 10 machine contexts
  switching in a star topology, saving extended state and signals
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_swapcontext_star10_ext_sig)
{
  run_swapcontext_memset_star(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, 10);

  //  printf("test_libhpx_mctx_swapcontext_star10_ext_sig,%ld,%.1f,%ld,%ld,%.1f\n", ts_runs, (double) mean_ts, max_ts, min_ts, stdev_ts);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: hpx_mctx_swapcontext with no flags and 1,000 machine 
  contexts switching in a star topology, saving extended state and
  signals
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_swapcontext_star1000_ext_sig)
{
  run_swapcontext_memset_star(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, 1000);

  //  printf("test_libhpx_mctx_swapcontext_star1000_ext_sig,%ld,%.1f,%ld,%ld,%.1f\n", ts_runs, (double) mean_ts, max_ts, min_ts, stdev_ts);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: hpx_mctx_swapcontext with no flags and 5,000 machine 
  contexts switching in a star topology, saving extended state and
  signals
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_swapcontext_star5000_ext_sig)
{
  run_swapcontext_memset_star(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, 5000);
  
  //  printf("test_libhpx_mctx_swapcontext_star5000_ext_sig,%ld,%.1f,%ld,%ld,%.1f\n", ts_runs, (double) mean_ts, max_ts, min_ts, stdev_ts);
}
END_TEST


