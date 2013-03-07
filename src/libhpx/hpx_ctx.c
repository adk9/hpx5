
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
#include "hpx_kthread.h"
#include "hpx_thread.h"


/*
 --------------------------------------------------------------------
  hpx_ctx_create

  Create & initialize a new HPX context.
 --------------------------------------------------------------------
*/

hpx_context_t * hpx_ctx_create(void) {
  hpx_context_t * ctx = NULL;
  long cores;
  long x;

  ctx = (hpx_context_t *) hpx_alloc(sizeof(hpx_context_t));
  if (ctx != NULL) {
    memset(ctx, 0, sizeof(hpx_context_t));

    /* context ID */
    ctx->cid = __ctx_next_id;
    __ctx_next_id += 1;

    /* kernel threads */
    cores = hpx_kthread_get_cores();
    if (cores <= 0) {
      __hpx_errno = HPX_ERROR_KTH_CORES;
      goto __hpx_ctx_create_FAIL;
    }

    ctx->kths = (hpx_kthread_t **) hpx_alloc(cores * sizeof(hpx_kthread_t *));
    if (ctx->kths != NULL) {
      for (x = 0; x < cores; x++) {
        ctx->kths[x] = hpx_kthread_create(hpx_kthread_seed_default);
        if (ctx->kths[x] == NULL) {
          goto __hpx_ctx_create_FAIL;
	} else {
          ctx->kths_count = cores;
	}
      }    
    } else {
      __hpx_errno = HPX_ERROR_NOMEM;
      goto __hpx_ctx_create_FAIL;
    }

    /* user thread queues */
    hpx_queue_init(&ctx->q_pend);
    hpx_queue_init(&ctx->q_exe);
    hpx_queue_init(&ctx->q_block);
    hpx_queue_init(&ctx->q_susp);
    hpx_queue_init(&ctx->q_term);
  } else {
    __hpx_errno = HPX_ERROR_NOMEM;
  }

  return ctx;

 __hpx_ctx_create_FAIL:
  hpx_free(ctx);
  ctx = NULL;

  return NULL;
}


/*
 --------------------------------------------------------------------
  hpx_ctx_destroy

  Destroy a previously allocated HPX context.
 --------------------------------------------------------------------
*/

void hpx_ctx_destroy(hpx_context_t * ctx) {
  hpx_thread_t * th;
  uint32_t x;

  /* destroy kernel threads */
  for (x = 0; x < ctx->kths_count; x++) {
    hpx_kthread_destroy(ctx->kths[x]);
  }  

  hpx_free(ctx);
}


/*
 --------------------------------------------------------------------
  hpx_ctx_get_id

  Get the ID of this HPX context.
 --------------------------------------------------------------------
*/

hpx_context_id_t hpx_ctx_get_id(hpx_context_t * ctx) {
  return ctx->cid;
}


/*
 --------------------------------------------------------------------
  _hpx_ctx_thread_queue_destroy

  Helper function to destroy a user thread queue.  Called from
  hpx_ctx_destroy().
 --------------------------------------------------------------------
*/

void _hpx_ctx_thread_queue_destroy(hpx_queue_t * q) {
  hpx_thread_t * th;

  while (hpx_queue_size(q) > 0) {
    th = (hpx_thread_t *) hpx_queue_pop(q);
    hpx_thread_destroy(th);
    th = NULL;
  }

  hpx_queue_destroy(q);
}
