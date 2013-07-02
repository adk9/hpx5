
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


#include <string.h>
#include "hpx/thread.h"
#include "hpx/error.h"
#include "hpx/mem.h"


/*
 --------------------------------------------------------------------
  hpx_thread_get_id

  Returns the Thread ID from the supplied thread data.
 --------------------------------------------------------------------
*/

hpx_thread_id_t hpx_thread_get_id(hpx_thread_t * th) {
  return th->tid;
}


/*
 --------------------------------------------------------------------
  hpx_thread_create

  Creates and initializes a thread with variadic arguments.
 --------------------------------------------------------------------
*/

hpx_thread_t * hpx_thread_create(hpx_context_t * ctx, void * func, void * args) {
  hpx_thread_t * th = NULL;
  hpx_thread_id_t th_id;

  /* see if we can reuse a terminated thread */
  hpx_kthread_mutex_lock(&ctx->mtx);
  th = hpx_queue_pop(&ctx->term_ths);

  /* increment the next thread ID */
  th_id = __thread_next_id;
  __thread_next_id += 1;  
  hpx_kthread_mutex_unlock(&ctx->mtx);

  /* if we didn't get a thread to reuse, create a stack and machine context area */
  if (th == NULL) {
    th = (hpx_thread_t *) hpx_alloc(sizeof(hpx_thread_t));
    if (th != NULL) {
      /* create a stack */
      th->ss = hpx_config_get_thread_stack_size(&ctx->cfg);
      th->stk = (void *) hpx_alloc(th->ss);
      if (th->stk == NULL) {
	__hpx_errno = HPX_ERROR_NOMEM;
	goto __hpx_thread_create_FAIL1;
      }

      /* create a machine context area */
      th->mctx = (hpx_mctx_context_t *) hpx_alloc(sizeof(hpx_mctx_context_t));
      if (th->mctx == NULL) {
        __hpx_errno = HPX_ERROR_NOMEM;
        goto __hpx_thread_create_FAIL2;
      }
    } else {
      __hpx_errno = HPX_ERROR_NOMEM;
      goto __hpx_thread_create_FAIL0;
    }
  }

  /* initialize the thread */
  th->ctx = ctx;
  th->tid = th_id;
  th->state = HPX_THREAD_STATE_CREATE;
  th->opts = HPX_THREAD_OPT_NONE;
  th->parent = hpx_thread_self();
  th->func = func;
  th->args = args;

  hpx_lco_future_init(&th->retval);
  hpx_list_init(&th->children);

  /* add this thread to its parent's list of children */
  if (th->parent != NULL) {
    hpx_list_push(&th->parent->children, th);
  }

  /* put this thread on a kernel thread's pending queue */
  /* if it's bound and has a parent, use its parent's kernel thread */
  if ((th->opts & HPX_THREAD_OPT_BOUND) && (th->parent != NULL)) {
    th->kth = th->parent->kth;
  } else {
    ctx->kths_idx = ((ctx->kths_idx + 1) % ctx->kths_count);
    th->kth = ctx->kths[ctx->kths_idx];
  }

  _hpx_kthread_sched(th->kth, th, HPX_THREAD_STATE_CREATE);

  return th;

 __hpx_thread_create_FAIL2:
  hpx_free(th->stk);

 __hpx_thread_create_FAIL1:
  hpx_free(th);

 __hpx_thread_create_FAIL0:
  return NULL;
}


/*
 --------------------------------------------------------------------
  hpx_thread_destroy

  Cleans up a previously created thread.
 --------------------------------------------------------------------
*/

void hpx_thread_destroy(hpx_thread_t * th) {
  if (th->stk != NULL) {
    hpx_free(th->mctx);
    hpx_free(th->stk);
  }

  hpx_list_destroy(&th->children);
  hpx_free(th);
}


/*
 --------------------------------------------------------------------
  hpx_thread_get_state

  Returns the queuing state of the thread.
 --------------------------------------------------------------------
*/

hpx_thread_state_t hpx_thread_get_state(hpx_thread_t * th) {
  return th->state;
}


/*
 --------------------------------------------------------------------
  hpx_thread_self

  Returns a pointer to thread data about the currently running 
  HPX thread.
 --------------------------------------------------------------------
*/

hpx_thread_t * hpx_thread_self(void) {
  struct _hpx_kthread_t * kth;
  hpx_thread_t * th = NULL;

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

void hpx_thread_join(hpx_thread_t * th, void ** value) {
  hpx_thread_t * self = hpx_thread_self();
  hpx_future_t * fut = &th->retval;
  uint8_t fut_st;

  /* poll the future until it's set */
  do {
    fut_st = fut->state;
    if (fut_st == HPX_LCO_FUTURE_UNSET) {
      hpx_thread_yield();
    }
  } while (fut_st == HPX_LCO_FUTURE_UNSET);

  /* copy its return value */
  if (value != NULL) {
    *value = fut->value;
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
  hpx_thread_t * th = hpx_thread_self();

  if ((th != NULL) && (th->kth != NULL)) {
    _hpx_kthread_sched(th->kth, th, HPX_THREAD_STATE_YIELD);
    hpx_mctx_swapcontext(th->mctx, th->kth->mctx, th->kth->mcfg, th->kth->mflags);
  }
}


/*
 --------------------------------------------------------------------
  hpx_thread_exit

  Exits the current thread and optionally passes a return value.
 --------------------------------------------------------------------
*/

void hpx_thread_exit(void * retval) {
  hpx_thread_t * th = hpx_thread_self();
  hpx_future_t * fut = &th->retval;

  if (retval != NULL) {
    fut->value = retval;
    hpx_lco_future_set(fut);    
  }

  _hpx_kthread_sched(th->kth, th, HPX_THREAD_STATE_TERMINATED);
  hpx_mctx_swapcontext(th->mctx, th->kth->mctx, th->kth->mcfg, th->kth->mflags);
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

void _hpx_thread_terminate(hpx_thread_t * th) {
  hpx_thread_t * child = NULL;

  /* trigger the return future */
  hpx_lco_future_set(&th->retval);

  /* set our state to TERMINATED */
  th->state = HPX_THREAD_STATE_TERMINATED;

  /* remove children and set their parent to their grandparent */
  do {
    child = hpx_list_pop(&th->children);
    if (child != NULL) {
      child->parent = th->parent;
      if (th->parent != NULL) {
        hpx_list_push(&th->parent->children, child);
      }
    }
  } while (child != NULL);

  /* remove this thread from its parent */
  if (th->parent != NULL) {
    hpx_list_delete(&th->parent->children, th);
  }

  /* if the thread is detached, destroy it.  otherwise, put it on the termination queue */
  if (th->opts & HPX_THREAD_OPT_DETACHED) {
    hpx_thread_destroy(th);
  } else {
    hpx_queue_push(&th->ctx->term_ths, th);
  }
}


/*
 --------------------------------------------------------------------
  hpx_thread_get_opt

  Gets the option flags for the specified thread.
 --------------------------------------------------------------------
*/

uint16_t hpx_thread_get_opt(hpx_thread_t * th) {
  return th->opts;
}


/*
 --------------------------------------------------------------------
  hpx_thread_set_opt

  Sets the option flags for the specified thread.
 --------------------------------------------------------------------
*/

void hpx_thread_set_opt(hpx_thread_t * th, uint16_t opts) {
  th->opts = opts;
}


/*
 --------------------------------------------------------------------
  hpx_thread_map_hash

  A rather naive hashing function for storing HPX Threads in maps.
 --------------------------------------------------------------------
*/

uint64_t hpx_thread_map_hash(hpx_map_t * map, void * ptr) {
  hpx_thread_t * th = (hpx_thread_t *) ptr;

  return (hpx_thread_get_id(th) % hpx_map_size(map));
}

/*
 --------------------------------------------------------------------
  hpx_thread_map_cmp

  A comparator for HPX Threads (stored in maps).
 --------------------------------------------------------------------
*/

bool hpx_thread_map_cmp(void * ptr1, void * ptr2) {
  hpx_thread_t * th1 = (hpx_thread_t *) ptr1;
  hpx_thread_t * th2 = (hpx_thread_t *) ptr2;

  return (hpx_thread_get_id(th1) == hpx_thread_get_id(th2));
}
