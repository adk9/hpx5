
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  "Kernel" Thread Functions
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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "hpx_kthread.h"
#include "hpx_error.h"
#include "hpx_mem.h"
#include "hpx_thread.h"


/*
 --------------------------------------------------------------------
  hpx_kthread_seed_default

  A default seed function for new kernel threads.
 --------------------------------------------------------------------
*/

void * hpx_kthread_seed_default(void * ptr) {
  hpx_kthread_t * kth = (hpx_kthread_t *) ptr;
  struct _hpx_thread_t * th = NULL;

  /* save a pointer to our data in TLS */
  pthread_setspecific(kth_key, kth);

  /* get our current machine context */
  hpx_mctx_getcontext(kth->mctx, kth->mcfg, kth->mflags); 

  /* enter our critical section */
  pthread_mutex_lock(&kth->mtx);

  /* if we are running and have something to do, get to it.  otherwise, wait. */
  while (kth->k_st != HPX_KTHREAD_STATE_STOPPED) {
    th = hpx_queue_pop(&kth->pend_q);
    if (th != NULL) {
      if (th->state == HPX_THREAD_STATE_INIT) {
        hpx_mctx_makecontext(th->mctx, kth->mctx, th->stk, th->ss, kth->mcfg, kth->mflags, th->func, 1, th->args);
      }

      th->state = HPX_THREAD_STATE_EXECUTING;
      kth->exec_th = th;

      pthread_mutex_unlock(&kth->mtx);
      hpx_mctx_swapcontext(kth->mctx, th->mctx, kth->mcfg, kth->mflags);
      pthread_mutex_lock(&kth->mtx);
    } else {
      pthread_cond_wait(&kth->k_c, &kth->mtx);
    }
  }

  /* leave */
  pthread_mutex_unlock(&kth->mtx);

  return NULL;  
}


/*
 --------------------------------------------------------------------
  hpx_kthread_create

  Creates a kernel thread and executes the provided seed function.
 --------------------------------------------------------------------
*/

hpx_kthread_t * hpx_kthread_create(hpx_kthread_seed_t seed, hpx_mconfig_t mcfg, uint64_t mflags) {
  hpx_kthread_t * kth = NULL;
  int err;

  /* allocate and init the handle */
  kth = (hpx_kthread_t *) hpx_alloc(sizeof(hpx_kthread_t));
  if (kth != NULL) {
    memset(kth, 0, sizeof(hpx_kthread_t));
    
    hpx_queue_init(&kth->pend_q);
    pthread_mutex_init(&kth->mtx, 0);
    pthread_cond_init(&kth->k_c, 0);    

    kth->k_st = HPX_KTHREAD_STATE_RUNNING;
    kth->mcfg = mcfg;
    kth->mflags = mflags;

    /* create a machine context buffer */
    kth->mctx = (hpx_mctx_context_t *) hpx_alloc(sizeof(hpx_mctx_context_t));
    if (kth->mctx == NULL) {
      pthread_cond_destroy(&kth->k_c);
      pthread_mutex_destroy(&kth->mtx);
      hpx_queue_destroy(&kth->pend_q);
      hpx_free(kth);

      return NULL;
    }

    /* create the thread */
    err = pthread_create(&kth->core_th, NULL, seed, (void *) kth);
    if (err != 0) {
      pthread_cond_destroy(&kth->k_c);
      pthread_mutex_destroy(&kth->mtx);
      hpx_queue_destroy(&kth->pend_q);
      hpx_free(kth->mctx);
      hpx_free(kth);
      kth = NULL;

      switch (err) {
        case EAGAIN:
          __hpx_errno = HPX_ERROR_KTH_MAX;
	  break;
        case EINVAL:
          __hpx_errno = HPX_ERROR_KTH_ATTR;
	  break;
        default:
	  __hpx_errno = HPX_ERROR_KTH_INIT;
	  break;
      }
    } 
  } else {
    __hpx_errno = HPX_ERROR_NOMEM;
  }

  return kth;
}


/*
 --------------------------------------------------------------------
  hpx_kthread_get_affinity

  Gets the logical CPU affinity for a given kernel thread.
 --------------------------------------------------------------------
*/

uint16_t hpx_kthread_get_affinity(hpx_kthread_t * kth) {
  return 0;
}


/*
 --------------------------------------------------------------------
  hpx_kthread_set_affinity

  Sets the logical CPU affinity for a given kernel thread.
 --------------------------------------------------------------------
*/

void hpx_kthread_set_affinity(hpx_kthread_t * kth, uint16_t aff) {

}


/*
 --------------------------------------------------------------------
  hpx_kthread_destroy

  Terminates and destroys a previously created kernel thread.
 --------------------------------------------------------------------
*/

void hpx_kthread_destroy(hpx_kthread_t * kth) {
  pthread_mutex_lock(&kth->mtx);
  kth->k_st = HPX_KTHREAD_STATE_STOPPED;
  pthread_cond_signal(&kth->k_c);
  pthread_mutex_unlock(&kth->mtx);

  pthread_join(kth->core_th, NULL);

  hpx_queue_destroy(&kth->pend_q);
  pthread_cond_destroy(&kth->k_c);
  pthread_mutex_destroy(&kth->mtx);
  hpx_free(kth->mctx);
  hpx_free(kth);
}


/*
 --------------------------------------------------------------------
  hpx_kthread_get_cores

  Returns the number of logical compute cores on this machine.
 --------------------------------------------------------------------
*/

long hpx_kthread_get_cores(void) {
  long cores = 0;

#ifdef __linux__                                      /* Linux */
  cores = sysconf(_SC_NPROCESSORS_ONLN);
#elif __APPLE__ && __MACH__                           /* Mac OS X */
  cores = sysconf(_SC_NPROCESSORS_ONLN);
#endif

  return cores;
}


/*
 --------------------------------------------------------------------
  _hpx_kthread_push_pending

  Pushes a new HPX thread onto the PENDING queue.
 --------------------------------------------------------------------
*/

void _hpx_kthread_push_pending(hpx_kthread_t * kth, struct _hpx_thread_t * th) {
  pthread_mutex_lock(&kth->mtx);
  hpx_queue_push(&kth->pend_q, th);
  pthread_mutex_unlock(&kth->mtx);  

  pthread_cond_signal(&kth->k_c);
}


/*
 --------------------------------------------------------------------
  __hpx_kthread_make_keys

  Helper function to create TLS keys for pthreads.
 --------------------------------------------------------------------
*/

static void __hpx_kthread_make_keys(void) {
  (void) pthread_key_create(&errno_key, NULL);
  (void) pthread_key_create(&kth_key, NULL);
}


/*
 --------------------------------------------------------------------
  _hpx_kthread_init

  Internal initialization function for kernel threads.
 --------------------------------------------------------------------
*/

void _hpx_kthread_init(void) {
  pthread_once(&__kthread_init_once, __hpx_kthread_make_keys);
}


/*
 --------------------------------------------------------------------
  hpx_kthread_self

  Returns a pointer to the currently running kernel thread's data.
 --------------------------------------------------------------------
*/

hpx_kthread_t * hpx_kthread_self(void) {
  return (hpx_kthread_t *) pthread_getspecific(kth_key);
}
