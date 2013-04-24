
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

hpx_context_t * hpx_ctx_create(hpx_config_t * cfg) {
  hpx_context_t * ctx = NULL;
  long cores;
  long x;

  ctx = (hpx_context_t *) hpx_alloc(sizeof(hpx_context_t));
  if (ctx != NULL) {
    memset(ctx, 0, sizeof(hpx_context_t));
    memcpy(&ctx->cfg, cfg, sizeof(hpx_config_t));

    /* context ID */
    ctx->cid = __ctx_next_id;
    __ctx_next_id += 1;

    /* kernel threads */
    _hpx_kthread_init();
    cores = hpx_config_get_cores(&ctx->cfg);
    if (cores <= 0) {
      __hpx_errno = HPX_ERROR_KTH_CORES;
      goto __hpx_ctx_create_FAIL;
    }

    /* kernel mutex */
    hpx_kthread_mutex_init(&ctx->mtx);

    /* terminated thread queue */
    hpx_queue_init(&ctx->term_ths);

    /* get the CPU configuration and set switching flags */
    ctx->mcfg = hpx_mconfig_get();

    ctx->kths = (hpx_kthread_t **) hpx_alloc(cores * sizeof(hpx_kthread_t *));
    if (ctx->kths != NULL) {
      for (x = 0; x < cores; x++) {
        ctx->kths[x] = hpx_kthread_create(ctx, hpx_kthread_seed_default, ctx->mcfg, hpx_config_get_switch_flags(&ctx->cfg));

        if (ctx->kths[x] == NULL) {
          goto __hpx_ctx_create_FAIL;
	} else {
	  hpx_kthread_set_affinity(ctx->kths[x], x);
	}
      }    
    } else {
      __hpx_errno = HPX_ERROR_NOMEM;
      goto __hpx_ctx_create_FAIL;
    }
  } else {
    __hpx_errno = HPX_ERROR_NOMEM;
  }

  /* set the core count and index */
  ctx->kths_count = cores;
  ctx->kths_idx = 0;

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

  /* destroy any remaining termianted threads */
  do {
    th = hpx_queue_pop(&ctx->term_ths);
    if (th != NULL) {
      hpx_thread_destroy(th);
    }
  } while (th != NULL);

  /* cleanup */
  hpx_free(ctx->kths);
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



