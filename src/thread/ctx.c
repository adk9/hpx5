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

#include "hpx/mem.h"
#include "hpx/error.h"
#include "hpx/kthread.h"
#include "hpx/thread.h"
#include "hpx/thread/ctx.h"
#include "hpx/utils/timer.h"
#include "init.h"                               /* libhpx_ctx_init() */
#include "debug.h"
#include "join.h"                               /* thread_join() */
#include "kthread.h"
#include "sync/barriers.h"                      /* sense-reversing barrier */
#include "sync/sync.h"                          /* sync ops */

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
  int cores = hpx_config_get_cores(cfg);
  if (cores <= 0) {
    dbg_printf("Configuration should specify positive cores, got %i\n", cores);
    __hpx_errno = HPX_ERROR_KTH_CORES;
    goto error0;
  }

  /* make sure that kernel threads are initialized in this address space
     LD: should this just be done in init?
   */
  libhpx_kthread_init();
  
  hpx_context_t *ctx = hpx_alloc(sizeof(*ctx));
  if (!ctx) {
    dbg_printf("Could not allocate a context.\n");
    __hpx_errno = HPX_ERROR_NOMEM;
    goto error0;
  }
  
  bzero(ctx, sizeof(*ctx));
  memcpy(&ctx->cfg, cfg, sizeof(hpx_config_t));

  ctx->cid = sync_fadd(&ctx_next_id, 1, SYNC_SEQ_CST);
  hpx_kthread_mutex_init(&ctx->mtx);
  hpx_queue_init(&ctx->term_ths);
  hpx_queue_init(&ctx->term_stks);
  hpx_queue_init(&ctx->term_lcos);
  ctx->mcfg = hpx_mconfig_get();

  /* hardware topology */
  //    hwloc_topology_init(&ctx->hw_topo);
  //    hwloc_topology_load(ctx->hw_topo);

  /* allocate the kthreads structures */
  ctx->kths = hpx_calloc(cores, sizeof(*ctx->kths));
  if (!ctx->kths) {
    dbg_printf("Could not allocate %i kthreads.\n", cores);
    __hpx_errno = HPX_ERROR_NOMEM;
    goto error1;
  }

  int id = 0;
  
  /* allocate a barrier for all of the threads in the context, which will be
     joined by the main thread after it's done initialization */
  ctx->barrier = sr_barrier_create(cores + 1);
  if (!ctx->barrier) {
    dbg_printf("Could not allocate an SR barrier for the context.\n");
    __hpx_errno = HPX_ERROR_NOMEM;
    goto error2;
  }
  
  /* create threads for each of the kthreads */
  for (id = 0; id < cores; ++id) {
    ctx->kths[id] = hpx_kthread_create(ctx, hpx_kthread_seed_default, ctx->mcfg,
                                       hpx_config_get_switch_flags(&ctx->cfg),
                                       id);
    if (!ctx->kths[id]) {
      dbg_printf("Could not create kthread %i in context.\n", id);
      goto error3;
    }
    hpx_kthread_set_affinity(ctx->kths[id], id);
  }
  
  /* set the core count and index */
  ctx->kths_count = cores;
  ctx->kths_idx = 0;

  /* initialize service thread futures */
  hpx_lco_future_init(&ctx->f_srv_susp);
  hpx_lco_future_init(&ctx->f_srv_rebal);

  /* create service threads: suspended thread pruner */
  ctx->srv_susp = hpx_calloc(cores, sizeof(ctx->srv_susp[0]));
  if (!ctx->srv_susp) {
    dbg_printf("Could not allocate service threads.\n");
    __hpx_errno = HPX_ERROR_NOMEM;
    goto error4;
  }

  switch (hpx_config_get_thread_suspend_policy(cfg)) {
  default:
    dbg_printf("Unknown thread suspension policy %i.\n",
               hpx_config_get_thread_suspend_policy(cfg));
    break;
  case HPX_CONFIG_THREAD_SUSPEND_SRV_LOCAL:
    for (int i = 0; i < cores; ++i) {
      hpx_thread_create(ctx,
                        HPX_THREAD_OPT_SERVICE_CORELOCAL,
                        libhpx_kthread_srv_susp_local,
                        ctx,
                        NULL,
                        &ctx->srv_susp[i]);
      ctx->srv_susp[i]->reuse->kth = ctx->kths[i];

      libhpx_kthread_sched(ctx->kths[i], ctx->srv_susp[i],
                           HPX_THREAD_STATE_CREATE, NULL, NULL, NULL); 
    }
    break;
  case HPX_CONFIG_THREAD_SUSPEND_SRV_GLOBAL:
    hpx_thread_create(ctx,
                      HPX_THREAD_OPT_SERVICE_COREGLOBAL,
                      libhpx_kthread_srv_susp_global,
                      ctx,
                      NULL,
                      &ctx->srv_susp[0]);
    break;
  }

  /* create service thread: workload rebalancer */
  hpx_thread_create(ctx,
                    HPX_THREAD_OPT_SERVICE_COREGLOBAL,
                    libhpx_kthread_srv_rebal,
                    ctx,
                    NULL,
                    &ctx->srv_rebal);
  return ctx;

error4:
  hpx_lco_future_destroy(&ctx->f_srv_rebal);
  hpx_lco_future_destroy(&ctx->f_srv_susp);
error3:
  sr_barrier_destroy(ctx->barrier);
error2:
  for (int i = 0; i < id; ++i)
    hpx_kthread_destroy(ctx->kths[i]);
error1:
  hpx_free(ctx);
error0:
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
      thread_join(ctx->srv_susp[x], NULL);
    }
    break;
  case HPX_CONFIG_THREAD_SUSPEND_SRV_GLOBAL:
    thread_join(ctx->srv_susp[0], NULL);
    break;
  default:
    break;
  }

  hpx_lco_future_set_state(&ctx->f_srv_rebal);
  thread_join(ctx->srv_rebal, NULL);

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
