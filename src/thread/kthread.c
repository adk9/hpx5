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

#ifdef __linux__
  #define _GNU_SOURCE
#endif

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include "hpx/ctx.h"
#include "hpx/kthread.h"
#include "hpx/error.h"
#include "hpx/mem.h"
#include "hpx/thread.h"
#include "kthread.h"

static pthread_key_t errno_key;
static pthread_key_t kth_key;

/*
 --------------------------------------------------------------------
 make_keys

  Helper function to create TLS keys for pthreads.
 --------------------------------------------------------------------
*/
static void make_keys(void) {
  (void) pthread_key_create(&errno_key, NULL);
  (void) pthread_key_create(&kth_key, NULL);
}

/*
 --------------------------------------------------------------------
  hpx_kthread_init

  Internal initialization function for kernel threads.
 --------------------------------------------------------------------
*/
void libhpx_kthread_init(void) {
    static pthread_once_t init_once = PTHREAD_ONCE_INIT;
    pthread_once(&init_once, make_keys);
}

/*
 --------------------------------------------------------------------
  libhpx_kthread_sched

  The HPX Thread Scheduler.
 --------------------------------------------------------------------
*/
void libhpx_kthread_sched(hpx_kthread_t *kth, hpx_thread_t *th, uint8_t state,
    void *target, bool (*pred)(void *, void *), void *arg) {
  /* hpx_thread_t *exec_th = kth->exec_th; */
  /* hpx_context_t *ctx = kth->ctx; */
  /* uint64_t cnt; */

  pthread_mutex_lock(&kth->mtx);

  /* if we have a thread specified, do something with it */
  if (th != NULL) {
    switch (state) {
      case HPX_THREAD_STATE_CREATE:
        th->state = HPX_THREAD_STATE_INIT;
        hpx_queue_push(&kth->pend_q, th);
        pthread_cond_signal(&kth->k_c);
        break;
      case HPX_THREAD_STATE_YIELD:
        th->state = HPX_THREAD_STATE_PENDING;
    if (target != NULL) {
            th->skip = (uint8_t) (uint64_t) target;
    }

        hpx_queue_push(&kth->pend_q, th);
    break;
      case HPX_THREAD_STATE_TERMINATED:
        th->state = HPX_THREAD_STATE_TERMINATED;
        break;
      case HPX_THREAD_STATE_SUSPENDED:
        th->state = HPX_THREAD_STATE_SUSPENDED;
        th->reuse->wait = target;
        th->reuse->func = (void (*)())pred;
        th->reuse->args = arg;
        hpx_queue_push(&kth->susp_q, th);
        break;
      default:
        break;
    }
  }

  pthread_mutex_unlock(&kth->mtx);
}


/*
 --------------------------------------------------------------------
  hpx_kthread_seed_default

  A default seed function for new kernel threads.
 --------------------------------------------------------------------
*/

void * hpx_kthread_seed_default(void *ptr) {
  hpx_kthread_t *kth = (hpx_kthread_t *) ptr;
  /* hpx_thread_t *th = NULL; */
  /* hpx_context_t *ctx = kth->ctx; */
  struct timespec ts;
  struct timeval tv;
  /* uint64_t susp_idx = 0; */
  /* uint64_t cnt; */

  /* save a pointer to our data in TLS */
  pthread_setspecific(kth_key, kth);

  /* get our current machine context */
  hpx_mctx_getcontext(kth->mctx, kth->mcfg, kth->mflags);

  /* enter our critical section */
  pthread_mutex_lock(&kth->mtx);

  /* if we are running and have something to do, get to it.  otherwise, wait. */
  while (kth->k_st != HPX_KTHREAD_STATE_STOPPED) {
    if (kth->exec_th != NULL) {
      switch (kth->exec_th->state) {
        case HPX_THREAD_STATE_YIELD:
      kth->exec_th->state = HPX_THREAD_STATE_PENDING;
      hpx_queue_push(&kth->pend_q, kth->exec_th);
      break;
      case HPX_THREAD_STATE_EXECUTING:
        _hpx_thread_terminate(kth->exec_th);
    break;
      case HPX_THREAD_STATE_TERMINATED:
    _hpx_thread_terminate(kth->exec_th);
        break;
      }
    }

    kth->exec_th = (hpx_thread_t *) hpx_queue_pop(&kth->pend_q);

    /* if we have an next thread, put it in the EXECUTING state */
    if (kth->exec_th != NULL) {
      switch (kth->exec_th->state) {
        case HPX_THREAD_STATE_INIT:
      kth->exec_th->state = HPX_THREAD_STATE_EXECUTING;
      hpx_mctx_makecontext(kth->exec_th->reuse->mctx, kth->mctx, kth->exec_th->reuse->stk, kth->exec_th->reuse->ss, kth->mcfg, kth->mflags, kth->exec_th->reuse->func, 1, kth->exec_th->reuse->args);
      break;
        case HPX_THREAD_STATE_PENDING:
      kth->exec_th->state = HPX_THREAD_STATE_EXECUTING;
      break;
        default:
      break;
      }

      pthread_mutex_unlock(&kth->mtx);
      hpx_mctx_swapcontext(kth->mctx, kth->exec_th->reuse->mctx, kth->mcfg, kth->mflags);
      pthread_mutex_lock(&kth->mtx);
    } else {
      gettimeofday(&tv, NULL);
      ts.tv_sec = tv.tv_sec;
      ts.tv_nsec = (tv.tv_usec * 1000) + 5;

      pthread_cond_timedwait(&kth->k_c, &kth->mtx, &ts);
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

hpx_kthread_t *hpx_kthread_create(hpx_context_t *ctx, hpx_kthread_seed_t seed,
                                  hpx_mconfig_t mcfg, uint64_t mflags) {
  /* pthread_mutexattr_t mtx_attr; */
  hpx_kthread_t *kth = NULL;
  int err;

  /* allocate and init the handle */
  kth = (hpx_kthread_t *) hpx_alloc(sizeof(hpx_kthread_t));
  if (kth != NULL) {
    memset(kth, 0, sizeof(hpx_kthread_t));

    hpx_queue_init(&kth->pend_q);
    hpx_queue_init(&kth->susp_q);

    hpx_kthread_mutex_init(&kth->mtx);
    pthread_cond_init(&kth->k_c, 0);

    kth->ctx = ctx;
    kth->k_st = HPX_KTHREAD_STATE_RUNNING;
    kth->mcfg = mcfg;
    kth->mflags = mflags;
    kth->pend_load = 0;

    /* create a machine context buffer */
    kth->mctx = (hpx_mctx_context_t *) hpx_alloc(sizeof(hpx_mctx_context_t));
    if (kth->mctx == NULL) {
      goto __hpx_kthread_create_FAIL1;
    }

    /* create the thread */
    err = pthread_create(&kth->core_th, NULL, seed, (void *) kth);
    if (err != 0) {
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

       goto __hpx_kthread_create_FAIL2;
    }
  } else {
    __hpx_errno = HPX_ERROR_NOMEM;
    goto __hpx_kthread_create_FAIL0;
  }

  return kth;

 __hpx_kthread_create_FAIL2:
  hpx_free(kth->mctx);

 __hpx_kthread_create_FAIL1:
  pthread_cond_destroy(&kth->k_c);
  hpx_kthread_mutex_destroy(&kth->mtx);

  hpx_queue_destroy(&kth->susp_q);
  hpx_queue_destroy(&kth->pend_q);

  hpx_free(kth);

 __hpx_kthread_create_FAIL0:
  return NULL;
}


/*
 --------------------------------------------------------------------
  hpx_kthread_set_affinity

  Sets the logical CPU affinity for a given kernel thread.
 --------------------------------------------------------------------
*/
void hpx_kthread_set_affinity(hpx_kthread_t *kth, uint16_t aff) {
#ifdef __linux__
  cpu_set_t cpuset;

  CPU_ZERO(&cpuset);
  CPU_SET(aff, &cpuset);

  pthread_setaffinity_np(kth->core_th, sizeof(cpu_set_t), &cpuset);
#endif
}


/*
 --------------------------------------------------------------------
  hpx_kthread_destroy

  Terminates and destroys a previously created kernel thread.
 --------------------------------------------------------------------
*/
void hpx_kthread_destroy(hpx_kthread_t *kth) {
  hpx_thread_t *th;

  /* shut down the kernel thread */
  pthread_mutex_lock(&kth->mtx);
  kth->k_st = HPX_KTHREAD_STATE_STOPPED;
  pthread_cond_signal(&kth->k_c);
  pthread_mutex_unlock(&kth->mtx);

  /* wait for our kernel thread to terminate */
  pthread_join(kth->core_th, NULL);

  /* destroy any remaining HPX threads in the pending queue */
  do {
    th = hpx_queue_pop(&kth->pend_q);
    if (th != NULL) {
      hpx_thread_destroy(th);
    }
  } while (th != NULL);

  /* destroy any remaining threads in the suspended queue */
  do {
    th = hpx_queue_pop(&kth->susp_q);
    if (th != NULL) {
      hpx_thread_destroy(th);
    }
  } while (th != NULL);

  /* cleanup */
  hpx_queue_destroy(&kth->susp_q);
  hpx_queue_destroy(&kth->pend_q);

  pthread_cond_destroy(&kth->k_c);
  hpx_kthread_mutex_destroy(&kth->mtx);

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
  hpx_kthread_self

  Returns a pointer to the currently running kernel thread's data.
 --------------------------------------------------------------------
*/
hpx_kthread_t *hpx_kthread_self(void) {
  return (hpx_kthread_t *) pthread_getspecific(kth_key);
}


/*
 --------------------------------------------------------------------
  hpx_kthread_mutex_init

  Initializes an HPX kernel mutex.
 --------------------------------------------------------------------
*/
void hpx_kthread_mutex_init(hpx_kthread_mutex_t *mtx) {
  pthread_mutexattr_t mtx_attr;

  pthread_mutexattr_init(&mtx_attr);
  pthread_mutexattr_settype(&mtx_attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init((pthread_mutex_t *) mtx, &mtx_attr);
}

/*
  --------------------------------------------------------------------
  hpx_kthread_mutex_destroy

  Destroys an HPX kernel mutex.
 --------------------------------------------------------------------
*/
void hpx_kthread_mutex_destroy(hpx_kthread_mutex_t *mtx) {
  pthread_mutex_destroy(mtx);
}

/*
 --------------------------------------------------------------------
  hpx_kthread_mutex_lock

  Locks an HPX kernel mutex.
 --------------------------------------------------------------------
*/
void hpx_kthread_mutex_lock(hpx_kthread_mutex_t *mtx) {
  pthread_mutex_lock((pthread_mutex_t *) mtx);
}


/*
 --------------------------------------------------------------------
  hpx_kthread_mutex_unlock

  Unlocks an HPX kernel mutex.
 --------------------------------------------------------------------
*/
void hpx_kthread_mutex_unlock(hpx_kthread_mutex_t *mtx) {
  pthread_mutex_unlock((pthread_mutex_t *) mtx);
}



/*
 --------------------------------------------------------------------
  hpx_kthread_srv_susp_local

  Service thread that wakes up suspended threads when the futures
  they are waiting are put into the SET state.
 --------------------------------------------------------------------
*/
void libhpx_kthread_srv_susp_local(void *ptr) {
  hpx_thread_wait_pred_t pred;
  hpx_context_t *ctx = (hpx_context_t *) ptr;
  hpx_future_t *fut = &ctx->f_srv_susp;
  hpx_kthread_t *kth = hpx_kthread_self();
  hpx_thread_t *th = NULL;
  uint64_t cnt;

  while (hpx_lco_future_isset(fut) == false) {
    hpx_kthread_mutex_lock(&kth->mtx);

    cnt = hpx_queue_size(&kth->susp_q);
    do {
      th = hpx_queue_pop(&kth->susp_q);
      if (th != NULL) {
        pred = (hpx_thread_wait_pred_t)th->reuse->func;
        if (pred(th->reuse->wait, th->reuse->args) == true) {
          th->state = HPX_THREAD_STATE_PENDING;
      hpx_queue_push(&kth->pend_q, th);
      cnt = 0;
    } else {
      hpx_queue_push(&kth->susp_q, th);
      cnt -= 1;
    }
      } else {
    cnt = 0;
      }
    } while (cnt > 0);

    hpx_kthread_mutex_unlock(&kth->mtx);
    hpx_thread_yield();
  }

  hpx_thread_exit(NULL);
}


/*
 --------------------------------------------------------------------
  libhpx_kthread_srv_susp_global

  (core global version)

  Service thread that wakes up suspended threads when the futures
  they are waiting are put into the SET state.
 --------------------------------------------------------------------
*/

void libhpx_kthread_srv_susp_global(void *ptr) {
  hpx_thread_wait_pred_t pred;
  hpx_context_t * ctx = (hpx_context_t *) ptr;
  hpx_future_t * fut = &ctx->f_srv_susp;
  hpx_kthread_t * kth = NULL;
  hpx_thread_t * th = NULL;
  uint64_t k_idx;
  uint64_t cnt;

  while (hpx_lco_future_isset(fut) == false) {
    for (k_idx = 0; k_idx < ctx->kths_count; k_idx++) {
      kth = ctx->kths[k_idx];

      hpx_kthread_mutex_lock(&kth->mtx);

      cnt = hpx_queue_size(&kth->susp_q);
      do {
        th = hpx_queue_pop(&kth->susp_q);
        if (th != NULL) {
            pred = (hpx_thread_wait_pred_t)th->reuse->func;
      if (pred(th->reuse->wait, th->reuse->args) == true) {
            th->state = HPX_THREAD_STATE_PENDING;
        hpx_queue_push(&kth->pend_q, th);
        cnt = 0;
      } else {
        hpx_queue_push(&kth->susp_q, th);
        cnt -= 1;
      }
        } else {
      cnt = 0;
        }
      } while (cnt > 0);

      hpx_kthread_mutex_unlock(&kth->mtx);
    }

    hpx_thread_yield_skip(ctx->kths_count);
  }

  hpx_thread_exit(NULL);
}



/*
 --------------------------------------------------------------------
  libhpx_kthread_srv_rebal

  Service thread that rebalances workload between cores.
 --------------------------------------------------------------------
*/

void libhpx_kthread_srv_rebal(void *ptr) {
  hpx_context_t * ctx = (hpx_context_t *) ptr;
  hpx_future_t * fut = &ctx->f_srv_rebal;
  /* hpx_kthread_t * kth_high = NULL; */
  /* hpx_kthread_t * kth_low = NULL; */
  /* hpx_kthread_t * kth = NULL; */
  /* hpx_thread_t * th = NULL; */
  /* uint64_t cnt_high = 0; */
  /* uint64_t cnt_low = 0; */
  /* uint64_t core_high = 0; */
  /* uint64_t core_low = 0; */
  /* uint64_t idx; */
  /* uint64_t cnt; */

  while (hpx_lco_future_isset(fut) == false) {
    //    for (idx = 0; idx < ctx->kths_count; idx++) {
    //      kth = ctx->kths[idx];
    //      hpx_kthread_mutex_lock(&kth->mtx);
    //    }
    //
    //    kth = ctx->kths[0];
    //    kth_high = kth;
    //    kth_low = kth;
    //
    //    for (idx = 0; idx < ctx->kths_count; idx++) {
    //      kth = ctx->kths[idx];
    //      cnt = hpx_queue_size(&kth->pend_q);
    //
    //      if (cnt_high < cnt) {
    //  cnt_high = cnt;
    //  kth_high = kth;
    //  core_high = idx;
    //      }
    //
    //      if (cnt_low >= cnt) {
    //  cnt_low = cnt;
    //  kth_low = kth;
    //  core_low = idx;
    //      }
    //    }
    //
    //    if ((cnt_high - cnt_low) > 3) {
    //      th = hpx_queue_pop(&kth_high->pend_q);
    //      if (th != NULL) {
    //  th->reuse->kth = kth_low;
    //  hpx_queue_push(&kth_low->pend_q, th);
    //      }
    //    }
    //
    //    for (idx = 0; idx < ctx->kths_count; idx++) {
    //      kth = ctx->kths[idx];
    //      hpx_kthread_mutex_unlock(&kth->mtx);
    //    }
    //
    hpx_thread_yield();
  }

  hpx_thread_exit(NULL);
}
