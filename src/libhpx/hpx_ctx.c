
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Scheduling Context Functions
  hpx_ctx.c

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
#include "hpx_mem.h"
#include "hpx_ctx.h"
#include "hpx_error.h"


/*
 --------------------------------------------------------------------
  hpx_ctx_create
 --------------------------------------------------------------------
*/

hpx_context_t * hpx_ctx_create(void) {
  hpx_context_t * ctx = NULL;

  ctx = (hpx_context_t *) hpx_alloc(sizeof(hpx_context_t));
  if (ctx != NULL) {
    memset(ctx, 0, sizeof(hpx_context_t));

    /* context ID */
    ctx->cid = __ctx_next_id;
    __ctx_next_id += 1;

    /* state heaps */
    hpx_heap_init(&ctx->q_pend);
    hpx_heap_init(&ctx->q_exe);
    hpx_heap_init(&ctx->q_block);
    hpx_heap_init(&ctx->q_susp);
    hpx_heap_init(&ctx->q_term);
  } else {
    __hpx_errno = HPX_ERROR_NOMEM;
  }

  return ctx;
}


/*
 --------------------------------------------------------------------
  hpx_ctx_destroy
 --------------------------------------------------------------------
*/

void hpx_ctx_destroy(hpx_context_t * ctx) {
  hpx_free(ctx);
}


/*
 --------------------------------------------------------------------
  hpx_ctx_get_id
 --------------------------------------------------------------------
*/

hpx_context_id_t hpx_ctx_get_id(hpx_context_t * ctx) {
  return ctx->cid;
}
