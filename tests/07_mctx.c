
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
#include "hpx_init.h"
#include "hpx_mctx.h"

#ifdef __APPLE__
  #include <mach/mach.h>
  #include <mach/mach_time.h>
#endif


hpx_mctx_context_t * mctx1;
hpx_mctx_context_t * mctx2;
int * context_counter;


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


extern void _fpu_crusher(hpx_mconfig_t mcfg, uint64_t mflags);
extern void _sse_crusher(hpx_mconfig_t mcfg, uint64_t mflags, uint32_t * xmm_vals);


void thread_seed(int a, int b, char c) {
  register_crusher(a, b, c);
}


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


void run_getcontext(uint64_t mflags) {
  hpx_mctx_context_t mctx;
  hpx_context_t * ctx;
  hpx_xmmreg_t * xmmreg;
  long double * x87reg;
  long double x87_epsilon = 0.000000000000001;
  long double x87_abs;
  uint32_t xmmvals[4];
  char msg[256];
  sigset_t sigs_old;
  sigset_t sigs;

  /* get a thread context */
  ctx = hpx_ctx_create();
  ck_assert_msg(ctx != NULL, "Could not get a thread context.");

  memset(&mctx, 0, sizeof(hpx_mctx_context_t));

  /* crush that mean old FPU */
  _fpu_crusher(ctx->mcfg, mflags);

  /* crush that fancy pants SSE */
  xmmvals[0] = 1234;
  xmmvals[1] = 5678;
  xmmvals[2] = 9012;
  xmmvals[3] = 3456;

  _sse_crusher(ctx->mcfg, mflags, xmmvals);

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

#ifdef __x86_64
  /* test for some easy stuff */
  ck_assert_msg(mctx.regs.rsp != 0, "Stack pointer was not saved.");
  ck_assert_msg(mctx.regs.rip != 0, "Instruction pointer was not saved.");

  /* test function call registers */
  sprintf(msg, "First argument passing register (RDI) was not saved (expected %d, got %d).", (uint64_t) &mctx, mctx.regs.rdi);
  ck_assert_msg(mctx.regs.rdi == (uint64_t) &mctx, msg);

  sprintf(msg, "Second argument passing register (RSI) was not saved (expected %d, got %d).", (uint64_t) ctx->mcfg, mctx.regs.rsi);
  ck_assert_msg(mctx.regs.rsi == (uint64_t) ctx->mcfg, msg);

  sprintf(msg, "Third argument passing register (RDX) was not saved (expected %d, got %d).", mflags, mctx.regs.rdx);
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
  }

  /* test crushed SSE unit */
  if ((mflags & HPX_MCTX_SWITCH_EXTENDED) && (ctx->mcfg & _HPX_MCONFIG_HAS_SSE)) {
    /* XMM0 should contain what we passed in */
    xmmreg = (hpx_xmmreg_t *) &mctx.regs.fpregs.xmms[0];
    sprintf(msg, "First dword of XMM0 was not saved (exptected 1234, got %d).", xmmreg->vec32[0]);
    ck_assert_msg((uint32_t) xmmreg->vec32[0] == 1234, msg);

    sprintf(msg, "Second dword of XMM0 was not saved (expected 5678, got %d).", xmmreg->vec32[1]);
    ck_assert_msg((uint32_t) xmmreg->vec32[1] == 5678, msg);

    sprintf(msg, "Third dword of XMM0 was not saved (expected 9012, got %d).", xmmreg->vec32[2]);
    ck_assert_msg((uint32_t) xmmreg->vec32[2] == 9012, msg);

    sprintf(msg, "Fourth dword of XMM0 was not saved (expected 3456, got %d).", xmmreg->vec32[3]);
    ck_assert_msg((uint32_t) xmmreg->vec32[3] == 3456, msg);
   
    /* XMM1 should contain values 0-1 in its high qword and 2-3 in the low qword */
    xmmreg++;
    sprintf(msg, "First dword of XMM1 was not saved (expected 9012, got %d).", xmmreg->vec32[0]);
    ck_assert_msg((uint32_t) xmmreg->vec32[0] == 9012, msg);

    sprintf(msg, "Second dword of XMM1 was not saved (expected 3456, got %d).", xmmreg->vec32[1]);
    ck_assert_msg((uint32_t) xmmreg->vec32[1] == 3456, msg);

    sprintf(msg, "Third dword of XMM1 was not saved (expected 1234, got %d).", xmmreg->vec32[2]);
    ck_assert_msg((uint32_t) xmmreg->vec32[2] == 1234, msg);

    sprintf(msg, "Fourth dword of XMM1 was not saved (expected 5678, got %d).", xmmreg->vec32[3]);
    ck_assert_msg((uint32_t) xmmreg->vec32[3] == 5678, msg);

    /* XMM2 should be 9012, 5678, 9012, 5678 */
    xmmreg++;
    sprintf(msg, "First dword of XMM2 was not saved (expected 9012, got %d).", xmmreg->vec32[0]);
    ck_assert_msg((uint32_t) xmmreg->vec32[0] == 9012, msg);

    sprintf(msg, "Second dword of XMM2 was not saved (expected 5678, got %d).", xmmreg->vec32[1]);
    ck_assert_msg((uint32_t) xmmreg->vec32[1] == 5678, msg);

    sprintf(msg, "Third dword of XMM2 was not saved (expected 9012, got %d).", xmmreg->vec32[2]);
    ck_assert_msg((uint32_t) xmmreg->vec32[2] == 9012, msg);

    sprintf(msg, "Fourth dword of XMM2 was not saved (expected 5678, got %d).", xmmreg->vec32[3]);
    ck_assert_msg((uint32_t) xmmreg->vec32[3] == 5678, msg);

    /* XMM3 should be 7778, 0, 0, 2222 */
    xmmreg++;
    sprintf(msg, "First dword of XMM3 was not saved (expected 7778, got %d).", xmmreg->vec32[0]);
    ck_assert_msg((uint32_t) xmmreg->vec32[0] == 7778, msg);

    sprintf(msg, "Second dword of XMM3 was not saved (expected 0, got %d).", xmmreg->vec32[1]);
    ck_assert_msg((uint32_t) xmmreg->vec32[1] == 0, msg);

    sprintf(msg, "Third dword of XMM3 was not saved (expected 0, got %d).", xmmreg->vec32[2]);
    ck_assert_msg((uint32_t) xmmreg->vec32[2] == 0, msg);

    sprintf(msg, "Fourth dword of XMM3 was not saved (expected 2222, got %d).", xmmreg->vec32[3]);
    ck_assert_msg((uint32_t) xmmreg->vec32[3] == 2222, msg);

    /* XMM4 should be 1234, 3456, 1234, 3456 */
    xmmreg++;
    sprintf(msg, "First dword of XMM4 was not saved (expected 1234, got %d).", xmmreg->vec32[0]);
    ck_assert_msg((uint32_t) xmmreg->vec32[0] == 1234, msg);

    sprintf(msg, "Second dword of XMM4 was not saved (expected 3456, got %d).", xmmreg->vec32[1]);
    ck_assert_msg((uint32_t) xmmreg->vec32[1] == 3456, msg);

    sprintf(msg, "Third dword of XMM4 was not saved (expected 1234, got %d).", xmmreg->vec32[2]);
    ck_assert_msg((uint32_t) xmmreg->vec32[2] == 1234, msg);

    sprintf(msg, "Fourth dword of XMM4 was not saved (expected 3456, got %d).", xmmreg->vec32[3]);
    ck_assert_msg((uint32_t) xmmreg->vec32[3] == 3456, msg);

    /* XMM5 should be the same as XMM4 */
    xmmreg++;
    sprintf(msg, "First dword of XMM5 was not saved (expected 1234, got %d).", xmmreg->vec32[0]);
    ck_assert_msg((uint32_t) xmmreg->vec32[0] == 1234, msg);

    sprintf(msg, "Second dword of XMM5 was not saved (expected 3456, got %d).", xmmreg->vec32[1]);
    ck_assert_msg((uint32_t) xmmreg->vec32[1] == 3456, msg);

    sprintf(msg, "Third dword of XMM5 was not saved (expected 1234, got %d).", xmmreg->vec32[2]);
    ck_assert_msg((uint32_t) xmmreg->vec32[2] == 1234, msg);

    sprintf(msg, "Fourth dword of XMM5 was not saved (expected 3456, got %d).", xmmreg->vec32[3]);
    ck_assert_msg((uint32_t) xmmreg->vec32[3] == 3456, msg);
  }

  /* test our signal set */
  if (mflags & HPX_MCTX_SWITCH_SIGNALS) {
    printf("sigs == %d\n", mctx.sigs);
    ck_assert_msg(sigismember(&mctx.sigs, SIGTERM) == 1, "Signals were not saved (expected SIGTERM).");
    ck_assert_msg(sigismember(&mctx.sigs, SIGABRT) == 1, "Signals were not saved (expected SIGABRT).");
    ck_assert_msg(sigismember(&mctx.sigs, SIGPIPE) == 1, "Signals were not saved (expected SIGPIPE).");
  }
#endif
  /* clean up */
  hpx_ctx_destroy(ctx);
}


void run_setcontext_counter(uint64_t mflags) {
  hpx_mctx_context_t mctx;
  hpx_context_t * ctx;
  char msg[128];

  /* get a thread context */
  ctx = hpx_ctx_create();
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


void run_makecontext_counter(uint64_t mflags, int mk_limit, void * func, int argc, ...) {
  hpx_context_t * ctx;
  char st1[8192] __attribute__((aligned (16)));
  char msg[128];
  va_list argv;

  /* allocate machine contexts */
  mctx1 = (hpx_mctx_context_t *) hpx_alloc(sizeof(hpx_mctx_context_t));
  ck_assert_msg(mctx1 != NULL, "Could not allocate machine context 1.");

  mctx2 = (hpx_mctx_context_t *) hpx_alloc(sizeof(hpx_mctx_context_t));
  ck_assert_msg(mctx2 != NULL, "Could not allocate machine context 2.");

  /* get a thread context */
  ctx = hpx_ctx_create();
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
  memcpy(mctx2, mctx1, sizeof(hpx_mctx_context_t));
  mctx2->sp = st1;
  mctx2->ss = sizeof(st1);
  mctx2->link = mctx1;
  hpx_mctx_makecontext_va(mctx2, ctx->mcfg, mflags, func, argc, &argv);
  va_end(argv);

  /* crush some registers */
  register_crusher(4,92, 'z');
 
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






