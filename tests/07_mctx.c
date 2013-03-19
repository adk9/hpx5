
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


void thread_seed(int a, int b, char c) {
  register_crusher(a, b, c);
}


void doubleincrement_context_counter(void) {
  *context_counter += 444;
  char msg[128];
  int x = 73;
}


/*
 --------------------------------------------------------------------
  TEST: context switching without saving extended (FPU) state
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_getcontext)
{
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
  hpx_mctx_getcontext(&mctx, ctx->mcfg, 0);

  *context_counter += 1;

  /* do something that (hopefully) changes the value of our registers */
  register_crusher(4,92, 'z'); 

  if (*context_counter < 1000000) {
    hpx_mctx_setcontext(&mctx, ctx->mcfg, 0);
  } 

  ck_assert_msg(*context_counter == 1000000, "Test counter was not incremented in context switch.");
#endif

  hpx_free(context_counter);
  hpx_ctx_destroy(ctx);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: context switching, saving extended (FPU) state
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_getcontext_ext)
{
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
  hpx_mctx_getcontext(&mctx, ctx->mcfg, HPX_MCTX_SWITCH_EXTENDED);  
  *context_counter += 1;

  /* do something that (hopefully) changes the value of our registers */
  register_crusher(4,92, 'z');

  if (*context_counter < 1000000) {
    hpx_mctx_setcontext(&mctx, ctx->mcfg, HPX_MCTX_SWITCH_EXTENDED);
  } 

  ck_assert_msg(*context_counter == 1000000, "Test counter was not incremented in context switch.");
#endif

  free(context_counter);
  hpx_ctx_destroy(ctx);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: context switching, saving extended (FPU) state and signals
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_getcontext_ext_sig)
{
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
  hpx_mctx_getcontext(&mctx, ctx->mcfg, HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS);  
  *context_counter += 1;

  /* do something that (hopefully) changes the value of our registers */
  register_crusher(4,92, 'z');

  if (*context_counter < 100) {
    hpx_mctx_setcontext(&mctx, __mcfg, HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS);
  } 

  ck_assert_msg(*context_counter == 100, "Test counter was not incremented in context switch.");
#endif

  free(context_counter);
  hpx_ctx_destroy(ctx);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: hpx_mctx_makecontext
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_makecontext)
{
  hpx_context_t * ctx;
  char st1[8192];
  char msg[128];

  /* allocate machine contexts */
  mctx1 = (hpx_mctx_context_t *) hpx_alloc(sizeof(hpx_mctx_context_t));
  ck_assert_msg(mctx1 != NULL, "Could not allocate machine context 1.");

  mctx2 = (hpx_mctx_context_t *) hpx_alloc(sizeof(hpx_mctx_context_t));
  ck_assert_msg(mctx2 != NULL, "Could not allocate machine context 2.");

  /* get a thread context */
  ctx = hpx_ctx_create();
  ck_assert_msg(ctx != NULL, "Could not get a thread context.");
 
  context_counter = (int *) malloc(sizeof(int));
  if (context_counter != NULL) {
    *context_counter = 0;
  }

  ck_assert_msg(context_counter != NULL, "Could not allocate a counter to test context switching.");
 
  memset(mctx1, 0, sizeof(hpx_mctx_context_t));
  memset(mctx2, 0, sizeof(hpx_mctx_context_t));

#ifdef __x86_64__
  hpx_mctx_getcontext(mctx1, ctx->mcfg, 0);  

  memcpy(mctx2, mctx1, sizeof(hpx_mctx_context_t));
  mctx2->sp = st1;
  mctx2->ss = sizeof(st1);
  mctx2->link = mctx1;
  hpx_mctx_makecontext(mctx2, ctx->mcfg, 0, doubleincrement_context_counter, 0);

  /* do something that (hopefully) changes the value of our registers */
  register_crusher(4,92, 'z');
 
  if (*context_counter < 44400) {
            hpx_mctx_setcontext(mctx2, ctx->mcfg, 0);
  } 

  ck_assert_msg(*context_counter == 44400, "Test counter was not incremented in context switch.");
#endif

  free(context_counter);
  hpx_ctx_destroy(ctx);

  hpx_free(mctx2);
  hpx_free(mctx1);
}
END_TEST


