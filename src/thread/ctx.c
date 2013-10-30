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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "init.h"                               /* libhpx_ctx_init() */
#include "kthread.h"
#include "hpx/mem.h"
#include "hpx/error.h"
#include "hpx/kthread.h"
#include "hpx/thread.h"
#include "hpx/thread/ctx.h"
#include "hpx/utils/timer.h"
#include "sync/sync.h"

/* the global next context ID */
static hpx_context_id_t ctx_next_id;

void
libhpx_ctx_init() {
    sync_store(&ctx_next_id, 0, SYNC_SEQ_CST);
}

/*
 --------------------------------------------------------------------
  hpx_ctx_create

  Create & initialize a new HPX context.
 --------------------------------------------------------------------
*/

hpx_context_t *
hpx_ctx_create(hpx_config_t *cfg)
{
  hpx_context_t *ctx = NULL;
  long cores = -1;
  long x;

  ctx = (hpx_context_t *) hpx_alloc(sizeof(hpx_context_t));
  if (ctx != NULL) {
    memset(ctx, 0, sizeof(hpx_context_t));
    memcpy(&ctx->cfg, cfg, sizeof(hpx_config_t));

    /* context ID */
    ctx->cid = sync_fadd(&ctx_next_id, 1, SYNC_SEQ_CST);

    /* kernel threads */
    libhpx_kthread_init();
    cores = hpx_config_get_cores(&ctx->cfg);
    if (cores <= 0) {
      __hpx_errno = HPX_ERROR_KTH_CORES;
      goto _hpx_ctx_create_FAIL0;
    }

    /* context mutex */
    hpx_kthread_mutex_init(&ctx->mtx);

    /* terminated thread queue */
    hpx_queue_init(&ctx->term_ths);

    /* reusable stack queue */
    hpx_queue_init(&ctx->term_stks);

    /* LCOs to destroy */
    hpx_queue_init(&ctx->term_lcos);

    /* get the CPU configuration and set switching flags */
    ctx->mcfg = hpx_mconfig_get();

    /* hardware topology */
    //    hwloc_topology_init(&ctx->hw_topo);
    //    hwloc_topology_load(ctx->hw_topo);

    ctx->kths = (hpx_kthread_t **) hpx_alloc(cores * sizeof(hpx_kthread_t *));
    if (ctx->kths != NULL) {
      for (x = 0; x < cores; x++) {
        ctx->kths[x] = hpx_kthread_create(ctx, hpx_kthread_seed_default, ctx->mcfg, hpx_config_get_switch_flags(&ctx->cfg));

        if (ctx->kths[x] == NULL) {
          goto _hpx_ctx_create_FAIL0;
    } else {
      hpx_kthread_set_affinity(ctx->kths[x], x);
    }
      }
    } else {
      __hpx_errno = HPX_ERROR_NOMEM;
      goto _hpx_ctx_create_FAIL0;
    }
  } else {
    __hpx_errno = HPX_ERROR_NOMEM;
  }

  /* set the core count and index */
  ctx->kths_count = cores;
  ctx->kths_idx = 0;

  /* initialize service thread futures */
  hpx_lco_future_init(&ctx->f_srv_susp);
  hpx_lco_future_init(&ctx->f_srv_rebal);

  /* create service threads: suspended thread pruner */
  ctx->srv_susp = (hpx_thread_t **) hpx_alloc(cores * sizeof(hpx_thread_t *));
  if (ctx->srv_susp == NULL) {
    __hpx_errno = HPX_ERROR_NOMEM;
    goto _hpx_ctx_create_FAIL1;
  }

  switch (hpx_config_get_thread_suspend_policy(cfg)) {
    case HPX_CONFIG_THREAD_SUSPEND_SRV_LOCAL:
      for (x = 0; x < cores; x++) {
        hpx_thread_create(ctx, HPX_THREAD_OPT_SERVICE_CORELOCAL, libhpx_kthread_srv_susp_local, ctx, &ctx->srv_susp[x]);
        ctx->srv_susp[x]->reuse->kth = ctx->kths[x];

        libhpx_kthread_sched(ctx->kths[x], ctx->srv_susp[x], HPX_THREAD_STATE_CREATE, NULL, NULL, NULL);
      }
      break;
    case HPX_CONFIG_THREAD_SUSPEND_SRV_GLOBAL:
      hpx_thread_create(ctx, HPX_THREAD_OPT_SERVICE_COREGLOBAL, libhpx_kthread_srv_susp_global, ctx, &ctx->srv_susp[0]);
      break;
    default:
      break;
  }

  /* create service thread: workload rebalancer */
  hpx_thread_create(ctx, HPX_THREAD_OPT_SERVICE_COREGLOBAL, libhpx_kthread_srv_rebal, ctx, &ctx->srv_rebal);

  return ctx;

 _hpx_ctx_create_FAIL1:
  for (x = 0; x < cores; x++) {
    hpx_kthread_destroy(ctx->kths[x]);
  }

 _hpx_ctx_create_FAIL0:
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
void
hpx_ctx_destroy(hpx_context_t *ctx)
{
  hpx_thread_reusable_t * th_ru;
  hpx_future_t * fut;
  hpx_thread_t *th;
  uint32_t x;

  /* stop service threads & wait */
  hpx_lco_future_set_state(&ctx->f_srv_susp);
  switch (hpx_config_get_thread_suspend_policy(&ctx->cfg)) {
    case HPX_CONFIG_THREAD_SUSPEND_SRV_LOCAL:
      for (x = 0; x < ctx->kths_count; x++) {
        hpx_thread_join(ctx->srv_susp[x], NULL);
      }
      break;
    case HPX_CONFIG_THREAD_SUSPEND_SRV_GLOBAL:
      hpx_thread_join(ctx->srv_susp[0], NULL);
      break;
    default:
      break;
  }

  hpx_lco_future_set_state(&ctx->f_srv_rebal);
  hpx_thread_join(ctx->srv_rebal, NULL);

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

  /* destroy remaining reusable stacks */
  do {
    th_ru = hpx_queue_pop(&ctx->term_stks);
    if (th_ru != NULL) {
      hpx_free(th_ru);
    }
  } while (th_ru != NULL);

  /* destroy LCOs */
  do {
    fut = hpx_queue_pop(&ctx->term_lcos);
    if (fut != NULL) {
      hpx_lco_future_destroy(fut);
      hpx_free(fut);
    }
  } while (fut != NULL);

  /* destroy hardware topology */
  //  hwloc_topology_destroy(ctx->hw_topo);

  hpx_lco_future_destroy(&ctx->f_srv_susp);

  /* cleanup */
  hpx_kthread_mutex_destroy(&ctx->mtx);

  hpx_free(ctx->kths);
  hpx_free(ctx);
}


/*
 --------------------------------------------------------------------
  hpx_ctx_get_id

  Get the ID of this HPX context.
 --------------------------------------------------------------------
*/
hpx_context_id_t
hpx_ctx_get_id(hpx_context_t *ctx)
{
  return ctx->cid;
}
