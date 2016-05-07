// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2016, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_APEX
# include <apex.h>
#endif

#include <hpx/builtins.h>
#include <libhpx/action.h>
#include <libhpx/config.h>
#include <libhpx/debug.h>
#include <libhpx/gas.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/memory.h>
#include <libhpx/network.h>
#include <libhpx/rebalancer.h>
#include <libhpx/scheduler.h>
#include "thread.h"

/// The pthread entry function for dedicated worker threads.
///
/// This is used by _create().
static void *_run(void *worker) {
  dbg_assert(here);
  dbg_assert(here->gas);
  dbg_assert(worker);

  self = worker;

  // Ensure that all of the threads have joined the address spaces.
  as_join(AS_REGISTERED);
  as_join(AS_GLOBAL);
  as_join(AS_CYCLIC);

#ifdef HAVE_APEX
  // let APEX know there is a new thread
  apex_register_thread("HPX WORKER THREAD");
#endif

  if (worker_start()) {
    dbg_error("failed to start processing lightweight threads.\n");
    return NULL;
  }

#ifdef HAVE_APEX
  // let APEX know the thread is exiting
  apex_exit_thread();
#endif

  // leave the global address space
  as_leave();

  // unbind self and return NULL
  return (self = NULL);
}

static int _create(worker_t *w) {
  int e = pthread_create(&w->thread, NULL, _run, w);
  if (e) {
    dbg_error("failed to start a scheduler worker pthread.\n");
    w->thread = 0;
    return e;
  }
  return LIBHPX_OK;
}

static void _join(worker_t *worker) {
  dbg_assert(worker);
  dbg_assert(worker->thread != pthread_self());
  int e = pthread_join(worker->thread, NULL);
  if (e) {
    dbg_error("cannot join worker thread %d (%s).\n", worker->id, strerror(e));
  }
}

struct scheduler *scheduler_new(const config_t *cfg) {
  thread_set_stack_size(cfg->stacksize);

  const int workers = cfg->threads;
  struct scheduler *s = NULL;
  size_t bytes = sizeof(*s) + workers * sizeof(worker_t);
  if (posix_memalign((void**)&s, HPX_CACHELINE_SIZE, bytes)) {
    dbg_error("could not allocate a scheduler.\n");
    return NULL;
  }

  sync_store(&s->stopped, SCHED_STOP, SYNC_RELEASE);
  sync_store(&s->next_tls_id, 0, SYNC_RELEASE);
  s->n_workers = workers;
  s->n_active = workers;

  s->state = SCHED_STOP;
  s->lock = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
  s->running = (pthread_cond_t)PTHREAD_COND_INITIALIZER;

  // initialize the worker data structures
  for (int i = 0, e = workers; i < e; ++i) {
    if (worker_init(&s->workers[i], s, i) != LIBHPX_OK) {
      dbg_error("failed to initialize a worker.\n");
      return NULL;
    }
  }

  // start all of the other worker threads
  for (int i = 1, e = workers; i < e; ++i) {
    if (_create(&s->workers[i]) != LIBHPX_OK) {
      scheduler_delete(s);
      return NULL;
    }
  }

  log_sched("initialized a new scheduler.\n");
  self = &s->workers[0];
  self->thread = pthread_self();
  log_sched("worker 0 ready.\n");
  return s;
}

void scheduler_delete(struct scheduler *sched) {
  // shut everyone down
  pthread_mutex_lock(&sched->lock);
  sched->state = SCHED_SHUTDOWN;
  pthread_cond_broadcast(&sched->running);
  pthread_mutex_unlock(&sched->lock);

  for (int i = 1, e = sched->n_workers; i < e; ++i) {
    worker_t *w = scheduler_get_worker(sched, i);
    if (w->thread) {
      _join(w);
    }
  }

  log_sched("joined worker threads.\n");

  for (int i = 0, e = sched->n_workers; i < e; ++i) {
    worker_fini(scheduler_get_worker(sched, i));
  }

  free(sched);

  self = NULL;
}

worker_t *scheduler_get_worker(struct scheduler *sched, int id) {
  assert(id >= 0);
  assert(id < sched->n_workers);
  worker_t *w = &sched->workers[id];
  assert(((uintptr_t)w & (HPX_CACHELINE_SIZE - 1)) == 0);
  return w;
}

int scheduler_restart(struct scheduler *sched) {
  int status;

  // notify everyone that they don't have to keep checking the lock.
  sync_store(&sched->stopped, SCHED_RUN, SYNC_RELEASE);

  // now switch the state of the running condition
  pthread_mutex_lock(&sched->lock);
  sched->state = SCHED_RUN;
  pthread_cond_broadcast(&sched->running);
  pthread_mutex_unlock(&sched->lock);

  status = worker_start();

  // now switch the state to stopped
  pthread_mutex_lock(&sched->lock);
  sched->state = SCHED_STOP;
  pthread_mutex_unlock(&sched->lock);

  return status;
}

void scheduler_stop(struct scheduler *sched, int code) {
  sync_store(&sched->stopped, code, SYNC_RELEASE);
}

int scheduler_is_stopped(struct scheduler *sched) {
  int stopped = sync_load(&sched->stopped, SYNC_ACQUIRE);
  return (stopped != SCHED_RUN);
}
