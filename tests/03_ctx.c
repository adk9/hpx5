
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Library Unit Test Harness - Memory Management
  02_mem.c

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


#include "hpx_ctx.h"


/*
 --------------------------------------------------------------------
  TEST: context creation & deletion
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_ctx_create)
{
  hpx_context_t * ctx = NULL;

  ctx = hpx_ctx_create();
  ck_assert_msg(ctx != NULL, "ctx is NULL");
  hpx_ctx_destroy(ctx);
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
  char msg[128];
  int x;

  for (x = 0; x < 100; x++) {
    ctx = hpx_ctx_create();

    sprintf(msg, "ctx is NULL on loop iteration %d", x);
    ck_assert_msg(ctx != NULL, msg);

    sprintf(msg, "ID of ctx is incorrect on loop iteration %d", x);
    ck_assert_msg(hpx_ctx_get_id(ctx) == x, msg);

    hpx_ctx_destroy(ctx);
    ctx = NULL;
  }
}
END_TEST
