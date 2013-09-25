
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Library Unit Test Harness - Thread Scheduler Context Management
  03_ctx.c

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


/*
 --------------------------------------------------------------------
  TEST: context creation & deletion
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_ctx_create)
{
  hpx_context_t * ctx = NULL;
  hpx_config_t cfg;

  hpx_config_init(&cfg);

  //  ctx = hpx_ctx_create(&cfg);
  //  ck_assert_msg(ctx != NULL, "ctx is NULL");
  //  hpx_ctx_destroy(ctx);
} 
END_TEST


/*
 --------------------------------------------------------------------
  TEST: get context ID
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_ctx_get_id)
{
  hpx_context_t * ctx = NULL;
  hpx_config_t cfg;
  char msg[128];
  int x;

  hpx_config_init(&cfg);

  for (x = 0; x < 10; x++) {
    ctx = hpx_ctx_create(&cfg);

    sprintf(msg, "ctx is NULL on loop iteration %d", x);
    ck_assert_msg(ctx != NULL, msg);

    sprintf(msg, "ID of ctx is incorrect on loop iteration %d", x);
    ck_assert_msg(hpx_ctx_get_id(ctx) == x+1, msg); /* hpx_init() always creates one ctx */

    hpx_ctx_destroy(ctx);
    ctx = NULL;
  }
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: non-default CPU core count
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_ctx_cores)
{
  hpx_context_t * ctx = NULL;
  hpx_config_t cfg;
  char msg[128];

  hpx_config_init(&cfg);
  hpx_config_set_cores(&cfg, 1);

  /* see if we can create a context with only one logical CPU core */
  ctx = hpx_ctx_create(&cfg);
  sprintf(msg, "Core count is incorrect in context (expected 1, got %d).", ctx->kths_count);
  ck_assert_msg(ctx->kths_count == 1, msg);

  hpx_ctx_destroy(ctx);
  ctx = NULL;

  /* make sure we're getting good defaults */
  hpx_config_init(&cfg);
  ctx = hpx_ctx_create(&cfg);
  sprintf(msg, "Core count is incorrect in context (expected %d, got %d).", (int) hpx_kthread_get_cores(), ctx->kths_count);
  ck_assert_msg(ctx->kths_count == hpx_kthread_get_cores(), msg);

  hpx_ctx_destroy(ctx);
  ctx = NULL;
}
END_TEST
