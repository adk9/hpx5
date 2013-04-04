
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
#include "hpx_thread.h"
#include "hpx_error.h"
#include "hpx_mem.h"


/*
 --------------------------------------------------------------------
  hpx_thread_create

  Creates and initializes a thread with variadic arguments.
 --------------------------------------------------------------------
*/

hpx_thread_t * hpx_thread_create(hpx_context_t * ctx, void * func, void * args) {
  hpx_thread_t * th = NULL;

  /* allocate the thread */
  th = (hpx_thread_t *) hpx_alloc(sizeof(hpx_thread_t));
  if (th != NULL) {
    /* initialize the thread */
    memset(th, 0, sizeof(hpx_thread_t));

    th->state = HPX_THREAD_STATE_INIT;
    th->func = func;
    th->args = args;
    
    /* create a stack to use */
    th->ss = 8192;
    th->stk = (void *) hpx_alloc(th->ss);
    if (th->stk == NULL) {
      hpx_free(th);
      return NULL;
    }

    /* create a machine context buffer */
    th->mctx = (hpx_mctx_context_t *) hpx_alloc(sizeof(hpx_mctx_context_t));
    if (th->mctx == NULL) {
      hpx_free(th->stk);
      hpx_free(th);
      return NULL;
    }

    /* get a kernel thread to run this on */
    /* we'll assume for now that we aren't pegging threads to a specific core */
    ctx->kths_idx = ((ctx->kths_idx + 1) % ctx->kths_count);
    th->kth = ctx->kths[ctx->kths_idx];

    _hpx_kthread_push_pending(th->kth, th);
  } else {
    __hpx_errno = HPX_ERROR_NOMEM;
  }

  return th;
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
