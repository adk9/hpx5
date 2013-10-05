/*
 ====================================================================
  High Performance ParalleX Library (libhpx)

  Thread Functions
  hpx_thread.c

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

#include <stdio.h>
#include <string.h>

#include "hpx/types.h"
#include "hpx/thread.h"
#include "hpx/thread/ctx.h"
#include "hpx/kthread.h"
#include "hpx/error.h"
#include "hpx/mem.h"

/* include the libhpx thread implementation */
#include "thread.h"
#include "kthread.h"
#include "sync/sync.h"

/* the next thread ID */
static hpx_thread_id_t thread_next_id;

/* called by hpx_init() to initialize this module---avoiding static
   constructors for now */
void libhpx_init_thread() {
    hpx_sync_store(&thread_next_id, 1, HPX_SYNC_SEQ_CST);
}

/*
 --------------------------------------------------------------------
  hpx_thread_get_id

  Returns the Thread ID from the supplied thread data.
 --------------------------------------------------------------------
*/
hpx_thread_id_t hpx_thread_get_id(hpx_thread_t *th) {
  return th->tid;
}

/*
 --------------------------------------------------------------------
  hpx_thread_create

  Creates and initializes a thread with variadic arguments.
 --------------------------------------------------------------------
*/
hpx_future_t *hpx_thread_create(hpx_context_t *ctx, uint16_t opts, void
    (*func)(void *), void *args, hpx_thread_t ** thp) {
  hpx_thread_reusable_t *th_ru = NULL;
  hpx_thread_t *th = NULL;
  hpx_thread_id_t th_id;

  /* see if we can reuse a terminated thread */
  if (opts == 0) {
    opts = HPX_THREAD_OPT_NONE;
  }

  hpx_kthread_mutex_lock(&ctx->mtx);

  /* increment the next thread ID */
  th_id = hpx_sync_fadd(&thread_next_id, 1, HPX_SYNC_SEQ_CST);

  /* see if we can get a reusable section from a terminated thread */
  th_ru = hpx_queue_pop(&ctx->term_stks);

  /* if we didn't get a reusable area, create one */
  if (th_ru == NULL) {
    th_ru = (hpx_thread_reusable_t *) hpx_alloc(sizeof(hpx_thread_reusable_t));
    if (th_ru == NULL) {
      hpx_kthread_mutex_unlock(&ctx->mtx);
      __hpx_errno = HPX_ERROR_NOMEM;
      goto _hpx_thread_create_FAIL0;
    }

    /* create a stack */
    th_ru->ss = hpx_config_get_thread_stack_size(&ctx->cfg);
    th_ru->stk = (void *) hpx_alloc(th_ru->ss);
    if (th_ru->stk == NULL) {
      hpx_kthread_mutex_unlock(&ctx->mtx);
      __hpx_errno = HPX_ERROR_NOMEM;
      goto _hpx_thread_create_FAIL1;
    }

    /* create a machine context switching area */
    th_ru->mctx = (hpx_mctx_context_t *) hpx_alloc(sizeof(hpx_mctx_context_t));
    if (th_ru->mctx == NULL) {
      hpx_kthread_mutex_unlock(&ctx->mtx);
      __hpx_errno = HPX_ERROR_NOMEM;
      goto _hpx_thread_create_FAIL2;
    }
  }

  /* create the non-reusable thread area */
  th = (hpx_thread_t *) hpx_alloc(sizeof(hpx_thread_t));
  if (th == NULL) {
    hpx_kthread_mutex_unlock(&ctx->mtx);
    __hpx_errno = HPX_ERROR_NOMEM;
    goto _hpx_thread_create_FAIL3;
  }

  /* initialize the thread */
  th->ctx = ctx;
  th->tid = th_id;
  th->state = HPX_THREAD_STATE_CREATE;
  th->opts = opts;
  th->parent = hpx_thread_self();
  th->skip = 0;

  th->reuse = th_ru;
  th->reuse->func = func;
  th->reuse->args = args;
  th->reuse->wait = NULL;

  th->f_ret = (hpx_future_t *) hpx_alloc(sizeof(hpx_future_t));
  if (th->f_ret == NULL) {
    hpx_kthread_mutex_unlock(&ctx->mtx);
    __hpx_errno = HPX_ERROR_NOMEM;
    goto _hpx_thread_create_FAIL4;
  }

  hpx_lco_future_init(th->f_ret);
  hpx_queue_push(&ctx->term_lcos, th->f_ret);

  /* put this thread on a kernel thread's pending queue */
  /* if it's bound and has a parent, use its parent's kernel thread */
  if ((opts & HPX_THREAD_OPT_SERVICE_CORELOCAL) != HPX_THREAD_OPT_SERVICE_CORELOCAL) {
    if ((th->opts & HPX_THREAD_OPT_BOUND) && (th->parent != NULL)) {
      th->reuse->kth = th->parent->reuse->kth;
    } else {
      ctx->kths_idx = ((ctx->kths_idx + 1) % ctx->kths_count);
      th->reuse->kth = ctx->kths[ctx->kths_idx];
    }
  }

  hpx_kthread_mutex_unlock(&ctx->mtx);

  if ((opts & HPX_THREAD_OPT_SERVICE_CORELOCAL) != HPX_THREAD_OPT_SERVICE_CORELOCAL) {
    libhpx_kthread_sched(th->reuse->kth, th, HPX_THREAD_STATE_CREATE, NULL, NULL, NULL);
  }

  if (thp != NULL) {
    *thp = th;
  }

  return th->f_ret;

 _hpx_thread_create_FAIL4:
  hpx_free(th);

 _hpx_thread_create_FAIL3:
  hpx_free(th_ru->mctx);

 _hpx_thread_create_FAIL2:
  hpx_free(th_ru->stk);

 _hpx_thread_create_FAIL1:
  hpx_free(th_ru);

 _hpx_thread_create_FAIL0:
  return NULL;
}


/*
 --------------------------------------------------------------------
  hpx_thread_destroy

  Cleans up a previously created thread.
 --------------------------------------------------------------------
*/
void hpx_thread_destroy(hpx_thread_t *th) {
  hpx_free(th);
}

/*
 --------------------------------------------------------------------
  hpx_thread_get_state

  Returns the queuing state of the thread.
 --------------------------------------------------------------------
*/
hpx_thread_state_t hpx_thread_get_state(hpx_thread_t *th) {
  return th->state;
}


/*
 --------------------------------------------------------------------
  hpx_thread_self

  Returns a pointer to thread data about the currently running
  HPX thread.
 --------------------------------------------------------------------
*/
hpx_thread_t *hpx_thread_self(void) {
  hpx_kthread_t *kth;
  hpx_thread_t *th = NULL;

  kth = hpx_kthread_self();
  if (kth != NULL) {
    th = kth->exec_th;
  }

  return th;
}


/*
 --------------------------------------------------------------------
  hpx_thread_join

  Wait until the specified thread terminates and get its return
  value (if any).
 --------------------------------------------------------------------
*/
void hpx_thread_join(hpx_thread_t *th, void **value) {
  hpx_thread_t *self = hpx_thread_self();
  hpx_future_t *fut = th->f_ret;

  /* poll the future until it's set */
  while (hpx_lco_future_isset(fut) == false) {
    if (self == NULL) {
      hpx_thread_yield();
    } else {
      hpx_thread_wait(fut);
    }
  }

  /* copy its return value */
  if (value != NULL) {
    *value = hpx_lco_future_get_value(fut);
  }
}


/*
 --------------------------------------------------------------------
  hpx_thread_yield

  Suspends execution of the current thread and allows another
  thread some CPU time.
 --------------------------------------------------------------------
*/
void hpx_thread_yield(void) {
  hpx_thread_t *th = hpx_thread_self();

  if ((th != NULL) && (th->reuse->kth != NULL)) {
    libhpx_kthread_sched(th->reuse->kth, th, HPX_THREAD_STATE_YIELD, NULL, NULL, NULL);
    hpx_mctx_swapcontext(th->reuse->mctx, th->reuse->kth->mctx, th->reuse->kth->mcfg, th->reuse->kth->mflags);
  }
}


/*
 --------------------------------------------------------------------
  hpx_thread_yield_skip

  Suspends execution of the current thread and allows another
  thread some CPU time.  Tells the scheduler to skip N number of
  rounds through the PENDING queue before scheduling this thread
  again.
 --------------------------------------------------------------------
*/
void hpx_thread_yield_skip(uint8_t sk) {
  hpx_thread_t *th = hpx_thread_self();

  if ((th != NULL) && (th->reuse->kth != NULL)) {
    libhpx_kthread_sched(th->reuse->kth, th, HPX_THREAD_STATE_YIELD, (void*)(uint64_t)sk, NULL, NULL);
    hpx_mctx_swapcontext(th->reuse->mctx, th->reuse->kth->mctx, th->reuse->kth->mcfg, th->reuse->kth->mflags);
  }
}



/*
 --------------------------------------------------------------------
  _hpx_lco_future_wait_pred

  Internal predicate function for use with _hpx_thread_wait()
 --------------------------------------------------------------------
*/

static bool future_wait_pred(void * target, void * arg) {
  return hpx_lco_future_isset((hpx_future_t *) target);
}

/*
 --------------------------------------------------------------------
  hpx_thread_wait

  Suspends execution of the current thread until an HPX Future is
  put into the SET state.
 --------------------------------------------------------------------
*/

void hpx_thread_wait(hpx_future_t *fut) {
  hpx_thread_t *th = hpx_thread_self();

  if (th != NULL) {
    if ((th->reuse->kth != NULL) && (fut != NULL)) {
        libhpx_kthread_sched(th->reuse->kth, th, HPX_THREAD_STATE_SUSPENDED, fut, future_wait_pred, NULL);
      hpx_mctx_swapcontext(th->reuse->mctx, th->reuse->kth->mctx, th->reuse->kth->mcfg, th->reuse->kth->mflags);
    }
  } else {
    /* if we have no TLS thread pointer, then we're the dispatch thread.  if so, spin */
    while (hpx_lco_future_isset(fut) == false) {
      hpx_thread_yield();
    }
  }
}


/*
 --------------------------------------------------------------------
  hpx_thread_exit

  Exits the current thread and optionally passes a return value.
 --------------------------------------------------------------------
*/
void hpx_thread_exit(void *retval) {
  hpx_thread_t *th = hpx_thread_self();
  if (th != NULL) {
    hpx_lco_future_set_value(th->f_ret, retval);

    libhpx_kthread_sched(th->reuse->kth, th, HPX_THREAD_STATE_TERMINATED, NULL, NULL, NULL);
    hpx_mctx_swapcontext(th->reuse->mctx, th->reuse->kth->mctx, th->reuse->kth->mcfg, th->reuse->kth->mflags);
  }
}


/*
 --------------------------------------------------------------------
  _hpx_thread_terminate

  Terminates a thread.

  1.  Removes itself as a child of its parent thread.
  2.  Sets the parent of its children to their grandparent, or
      NULL if it's the main thread.
  3.  By default, pushes itself onto the thread context's
      termination queue.
  4.  If the thread is detached, it destroys itself instead of
      requeuing.
  5.  Sets its state to TERMINATED just in case the scheduler
      hasn't already done this (it should have).
 --------------------------------------------------------------------
*/
void _hpx_thread_terminate(hpx_thread_t *th) {
  hpx_kthread_mutex_lock(&th->ctx->mtx);

  /* trigger the return future */
  hpx_lco_future_set_state(th->f_ret);

  /* set our state to TERMINATED */
  th->state = HPX_THREAD_STATE_TERMINATED;

  /* push the reusable thread area onto the reuse queue */
  hpx_queue_push(&th->ctx->term_stks, th->reuse);
  th->reuse = NULL;

  /* if the thread is detached, destroy it.  otherwise, put it on the termination queue */
  if (th->opts & HPX_THREAD_OPT_DETACHED) {
    hpx_thread_destroy(th);
  } else {
    hpx_queue_push(&th->ctx->term_ths, th);
  }

  hpx_kthread_mutex_unlock(&th->ctx->mtx);
}


/*
 --------------------------------------------------------------------
  _hpx_thread_wait

  Internal function for threads that wait on something other
  than their return futures.
 --------------------------------------------------------------------
*/

void _hpx_thread_wait(void * target, hpx_thread_wait_pred_t pred, void * arg) {
  hpx_thread_t *th = hpx_thread_self();

  if ((th != NULL) && (th->reuse->kth != NULL) && (target != NULL)) {
    libhpx_kthread_sched(th->reuse->kth, th, HPX_THREAD_STATE_SUSPENDED, target, pred, arg);
    hpx_mctx_swapcontext(th->reuse->mctx, th->reuse->kth->mctx, th->reuse->kth->mcfg, th->reuse->kth->mflags);
  }
}


/*
 --------------------------------------------------------------------
  hpx_thread_get_opt

  Gets the option flags for the specified thread.
 --------------------------------------------------------------------
*/
uint16_t hpx_thread_get_opt(hpx_thread_t *th) {
  return th->opts;
}


/*
 --------------------------------------------------------------------
  hpx_thread_set_opt

  Sets the option flags for the specified thread.
 --------------------------------------------------------------------
*/
void hpx_thread_set_opt(hpx_thread_t *th, uint16_t opts) {
  th->opts = opts;
}


/*
 --------------------------------------------------------------------
  hpx_thread_map_hash

  A rather naive hashing function for storing HPX Threads in maps.
 --------------------------------------------------------------------
*/
uint64_t hpx_thread_map_hash(hpx_map_t *map, void *ptr) {
  hpx_thread_t *th = (hpx_thread_t *) ptr;

  return (hpx_thread_get_id(th) % hpx_map_size(map));
}

/*
 --------------------------------------------------------------------
  hpx_thread_map_cmp

  A comparator for HPX Threads (stored in maps).
 --------------------------------------------------------------------
*/

bool hpx_thread_map_cmp(void *ptr1, void *ptr2) {
  hpx_thread_t *th1 = (hpx_thread_t *) ptr1;
  hpx_thread_t *th2 = (hpx_thread_t *) ptr2;

  return (hpx_thread_get_id(th1) == hpx_thread_get_id(th2));
}
