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

#include <assert.h>
#include <string.h>

#include "hpx/mem.h"
#include "hpx/error.h"
#include "hpx/kthread.h"
#include "hpx/thread.h"
#include "hpx/thread/ctx.h"
#include "hpx/utils/timer.h"
#include "ctx.h"
#include "init.h"                               /* libhpx_ctx_init() */
#include "debug.h"
#include "join.h"                               /* thread_join() */
#include "kthread.h"
#include "sync/barriers.h"                      /* sense-reversing barrier */
#include "sync/sync.h"                          /* sync ops */

/**
 * A singly-linked list node for our callback list.
 */
struct callback_list_node {
  callback_list_node_t *next;
  void (*callback)(void *);
  void *args;
};

static void init(callback_list_t *list) {
  tatas_init(&list->lock);
  list->head = NULL;
  list->tail = NULL;
}

/**
 * Append a callback to a list.
 */
static void append(callback_list_t *list, void (*callback)(void*), void *args) {
  dbg_assert_precondition(list);
  dbg_assert_precondition(callback);
  callback_list_node_t *node = malloc(sizeof(*node));
  assert(node && "Could not allocate a node for a callback list.");
  node->callback = callback;
  node->next = NULL;
  node->args = args;
  tatas_acquire(&list->lock);
  if (list->tail)                               /* no dummy nodes */
    list->tail->next = node;
  else
    list->head = node;
  list->tail = node;
  tatas_release(&list->lock);
}

/**
 * Prepend a callback to a list.
 */
static void prepend(callback_list_t *list, void (*callback)(void*),
                    void *args) {
  dbg_assert_precondition(list);
  dbg_assert_precondition(callback);
  callback_list_node_t *node = malloc(sizeof(*node));
  assert(node && "Could not allocate a node for a callback list.");
  node->callback = callback;
  node->next = NULL;
  node->args = args;
  tatas_acquire(&list->lock);
  node->next = list->head;
  list->head = node;
  if (!list->tail)
    list->tail = node;
  tatas_release(&list->lock);
}

/**
 * Execute all of the callbacks in a list.
 */
static void do_all(callback_list_t *list) {
  dbg_assert_precondition(list);
  tatas_acquire(&list->lock);
  for (callback_list_node_t *i = list->head; i; i = i->next)
    i->callback(i->args);
  tatas_release(&list->lock);
}

/**
 * Free all of the nodes in a callback list.
 */
static void free_all(callback_list_t *list) {
  dbg_assert_precondition(list);
  callback_list_node_t *n = NULL;
  tatas_acquire(&list->lock);
  while ((n = list->head)) {
    list->head = list->head->next;
    free(n);
  }
  list->tail = list->head;
  tatas_release(&list->lock);
}


/**
 * Add an initialization callback (for all kthreads in this address
 * space). Intializers are run in FIFO order.
 */
void ctx_add_kthread_init(hpx_context_t *ctx, void (*callback)(void*),
                          void *args) {
  dbg_assert_precondition(ctx);
  dbg_assert_precondition(callback);
  append(&ctx->kthread_on_init, callback, args);
}

/**
 * Add a finalization callback (for all kthreads in this address
 * space). Finalizers are run in LIFO order.
 */
void ctx_add_kthread_fini(hpx_context_t *ctx, void (*callback)(void*),
                          void * args) {
  dbg_assert_precondition(ctx);
  dbg_assert_precondition(callback);
  prepend(&ctx->kthread_on_fini, callback, args);
}

/* the global next context ID */
static hpx_context_id_t ctx_next_id;

void
libhpx_ctx_init() {
    sync_store(&ctx_next_id, 0, SYNC_SEQ_CST);
}

static void destroy_kthreads(hpx_context_t *ctx) {
  dbg_assert_precondition(ctx);
  for (int i = 0, e = ctx->kths_count; i < e; ++i)
    hpx_kthread_delete(ctx->kths[i]);
}

static int create_kthreads(hpx_context_t *ctx) {
  dbg_assert_precondition(ctx);
  for (int i = 0, e = ctx->kths_count; i < e; ++i) {
    ctx->kths[i] = hpx_kthread_new(ctx, i);
    if (!ctx->kths[i]) {
      dbg_printf("Could not create kthread %i in context.\n", i);
      return __hpx_errno;
    }
  }

  return HPX_SUCCESS;
}

static int create_suspension_service(hpx_context_t *ctx) {
  uint8_t policy = hpx_config_get_thread_suspend_policy(&ctx->cfg);

  if (policy == HPX_CONFIG_THREAD_SUSPEND_SRV_GLOBAL)
    return hpx_thread_create(ctx, HPX_THREAD_OPT_SERVICE_COREGLOBAL,
                             libhpx_kthread_srv_susp_global, ctx, NULL,
                             &ctx->srv_susp[0]);

  if (policy != HPX_CONFIG_THREAD_SUSPEND_SRV_LOCAL) {
    dbg_printf("Unknown thread suspension policy.\n");
    return HPX_SUCCESS;
  }


  for (int i = 0, e = ctx->kths_count; i < e; ++i) {
    int err = hpx_thread_create(ctx, HPX_THREAD_OPT_SERVICE_CORELOCAL,
                                libhpx_kthread_srv_susp_local, ctx, NULL,
                                &ctx->srv_susp[i]);
    if (err) {
      dbg_print_error(err, "Could not start local suspsension service.");
      return err;
    }

    ctx->srv_susp[i]->reuse->kth = ctx->kths[i];
    libhpx_kthread_sched(ctx->kths[i], ctx->srv_susp[i],
                         HPX_THREAD_STATE_CREATE, NULL, NULL, NULL);
  }

  return HPX_SUCCESS;
}

static int create_rebalance_service(hpx_context_t *ctx) {
  return hpx_thread_create(ctx, HPX_THREAD_OPT_SERVICE_COREGLOBAL,
                           libhpx_kthread_srv_rebal,
                           ctx, NULL, &ctx->srv_rebal);
}

/**
 * The argument type for the kthread-entry function. It captures the "seed"
 * function that the thread should run, with the seed arguments, as well as a
 * barrier that the kthread needs to wait on before doing anything.
 */
typedef struct {
  hpx_context_t      *ctx;
  void    *(*seed)(void*);
  hpx_kthread_t   *thread;
  sr_barrier_t   *barrier;
} entry_t;

/**
 * Quick utiliy that extracts the seed function and arguments from the entry
 * structure, and calls them. We expect this to be inlined into entry.
 */
static void *call(entry_t *args) {
  return args->seed(args->thread);
}

/**
 * All of our kthreads enter using this function. It performs all relevant
 * kthread dynamic initialization, like calling all of the on_init callbacks.
 */
static void *entry_stub(void *args) {
  entry_t *e = args;

  /* wait for everyone else---in particular, make sure that all of the
   * initializer and finalizer callbacks have been registered */
  sr_barrier_join(e->barrier, e->thread->tid);

  /* run all of the initializers */
  do_all(&e->ctx->kthread_on_init);

  /* set the finalizers as to-be-run on pthread_exit */
  void *result = NULL;
  pthread_cleanup_push((void (*)(void*))&do_all, &e->ctx->kthread_on_fini);
  result = call(e);
  pthread_cleanup_pop(1);
  return result;
}

static void start_kthreads(hpx_context_t *ctx) {
  for (int i = 0, e = ctx->kths_count; i < e; ++i)
    hpx_kthread_start(ctx->kths[i], entry_stub, ctx->kths_args[i]);
}

static int create_arguments(hpx_context_t *ctx) {
  for (int i = 0, e = ctx->kths_count; i < e; ++i) {
    entry_t *entry = hpx_alloc(sizeof(*entry));
    if (!entry) {
      dbg_printf("Could not allocate a kthread argument structure\n");
      return (__hpx_errno = HPX_ERROR_NOMEM);
    }

    entry->ctx = ctx;
    entry->seed = hpx_kthread_seed_default;
    entry->thread = ctx->kths[i];
    entry->barrier = ctx->barrier;
    ctx->kths_args[i] = entry;
  }
  return HPX_SUCCESS;
}

static void destroy_arguments(hpx_context_t *ctx) {
  for (int i = 0, e = ctx->kths_count; i < e; ++i)
    hpx_free(ctx->kths_args[i]);
}

/*
 --------------------------------------------------------------------
  hpx_ctx_create

  Create & initialize a new HPX context.
 --------------------------------------------------------------------
*/
hpx_context_t *hpx_ctx_create(hpx_config_t *cfg) {
  /* make sure that kernel threads are initialized in this address space
     LD: should this just be done in init?
   */
  libhpx_kthread_init();

  int cores = hpx_config_get_cores(cfg);
  if (cores <= 0) {
    dbg_printf("Configuration should specify positive cores, got %i\n", cores);
    __hpx_errno = HPX_ERROR_KTH_CORES;
    goto unwind0;
  }

  /* allocate a context descriptor */
  hpx_context_t *ctx = hpx_alloc(sizeof(*ctx));
  if (!ctx) {
    dbg_printf("Could not allocate a context.\n");
    __hpx_errno = HPX_ERROR_NOMEM;
    goto unwind0;
  }
  bzero(ctx, sizeof(*ctx));

  /* allocate the kthreads structure */
  ctx->kths = hpx_calloc(cores, sizeof(*ctx->kths));
  if (!ctx->kths) {
    dbg_printf("Could not allocate %i kthreads.\n", cores);
    __hpx_errno = HPX_ERROR_NOMEM;
    goto unwind1;
  }

    /* allocate the kthread arguments */
  ctx->kths_args = hpx_calloc(cores, sizeof(*ctx->kths_args));
  if (!ctx->kths) {
    dbg_printf("Could not allocate %i argument structures.\n", cores);
    __hpx_errno = HPX_ERROR_NOMEM;
    goto unwind2;
  }

  /* create service threads: suspended thread pruner */
  ctx->srv_susp = hpx_calloc(cores, sizeof(ctx->srv_susp[0]));
  if (!ctx->srv_susp) {
    dbg_printf("Could not allocate service threads.\n");
    __hpx_errno = HPX_ERROR_NOMEM;
    goto unwind3;
  }

  /* allocate a barrier for all of the threads in the context, which will be
     joined by the main thread after it's done initialization */
  ctx->barrier = sr_barrier_new(cores + 1);
  if (!ctx->barrier) {
    dbg_printf("Could not allocate an SR barrier for the context.\n");
    __hpx_errno = HPX_ERROR_NOMEM;
    goto unwind4;
  }

  /* intialize the context (already zeroed) */
  ctx->cid = sync_fadd(&ctx_next_id, 1, SYNC_SEQ_CST);
  ctx->kths_count = cores;
  ctx->kths_idx = 0;
  hpx_kthread_mutex_init(&ctx->mtx);
  memcpy(&ctx->cfg, cfg, sizeof(*cfg));
  ctx->mcfg = hpx_mconfig_get();
  hpx_queue_init(&ctx->term_ths);
  hpx_queue_init(&ctx->term_stks);
  hpx_queue_init(&ctx->term_lcos);

  hpx_lco_future_init(&ctx->f_srv_susp);
  hpx_lco_future_init(&ctx->f_srv_rebal);

  init(&ctx->kthread_on_init);
  init(&ctx->kthread_on_fini);

  if (create_kthreads(ctx))
    goto unwind5;

  if (create_suspension_service(ctx))
    goto unwind5;

  if (create_rebalance_service(ctx))
    goto unwind5;

  if (create_arguments(ctx))
    goto unwind6;

  start_kthreads(ctx);

  return ctx;

unwind6:
  destroy_arguments(ctx);
unwind5:
  destroy_kthreads(ctx);
  free_all(&ctx->kthread_on_fini);
  free_all(&ctx->kthread_on_init);
  hpx_lco_future_destroy(&ctx->f_srv_rebal);
  hpx_lco_future_destroy(&ctx->f_srv_susp);
  hpx_queue_destroy(&ctx->term_lcos);
  hpx_queue_destroy(&ctx->term_stks);
  hpx_queue_destroy(&ctx->term_ths);
  hpx_kthread_mutex_destroy(&ctx->mtx);
  sr_barrier_delete(ctx->barrier);
unwind4:
  hpx_free(ctx->srv_susp);
unwind3:
  hpx_free(ctx->kths_args);
unwind2:
  hpx_free(ctx->kths);
unwind1:
  hpx_free(ctx);
unwind0:
  return NULL;
}

void ctx_start(hpx_context_t *ctx) {
  sr_barrier_join(ctx->barrier, ctx->kths_count);

  /* This is a little bit weird---the thread calling this routine isn't really a
     kthread, but it's used off-and-on as a kthread by various parts of the
     code. We run kthread initializers here to make sure that any local data
     structures that the rest of the system assumes are initialized have really
     been initialized. */
  do_all(&ctx->kthread_on_init);
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

  /* Just like during initialization, we have to run the kthread finalizers in
     this thread. This is stupid, and depends on the fact that the same hardware
     thread runs both ctx_create and ctx_destroy. */
  do_all(&ctx->kthread_on_fini);

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
  for (x = 0; x < ctx->kths_count; x++)
    hpx_kthread_delete(ctx->kths[x]);

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


  free_all(&ctx->kthread_on_fini);
  free_all(&ctx->kthread_on_init);

  destroy_arguments(ctx);
  hpx_free(ctx->kths_args);
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
