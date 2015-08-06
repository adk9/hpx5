// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
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
#include <hpx/builtins.h>
#include <libhpx/action.h>
#include <libhpx/config.h>
#include <libhpx/debug.h>
#include <libhpx/libhpx.h>
#include <libhpx/memory.h>
#include <libhpx/scheduler.h>
#include "thread.h"

static void _bind_self(worker_t *worker) {
  dbg_assert(worker);

  if (self && self != worker) {
    dbg_error("HPX does not permit worker structure switching.\n");
  }
  self = worker;
  self->thread = pthread_self();
}

/// The pthread entry function for dedicated worker threads.
///
/// This is used by _create().
static void *_run(void *worker) {
  dbg_assert(here);
  dbg_assert(here->gas);
  dbg_assert(worker);

  _bind_self(worker);

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

  // leave the global address space
  as_leave();

  // unbind self and return NULL
  return (self = NULL);
}

static int _create(worker_t *worker, const config_t *cfg) {
  pthread_t thread;

  int e = pthread_create(&thread, NULL, _run, worker);
  if (e) {
    dbg_error("failed to start a scheduler worker pthread.\n");
    return e;
  }
  return LIBHPX_OK;
}

static void _join(worker_t *worker) {
  dbg_assert(worker);

  if (worker->thread == pthread_self()) {
    return;
  }

  int e = pthread_join(worker->thread, NULL);
  if (e) {
    dbg_error("cannot join worker thread %d (%s).\n", worker->id, strerror(e));
  }
}

static void _cancel(worker_t *worker) {
  dbg_assert(worker);
  dbg_assert(worker->thread != pthread_self());
  if (pthread_cancel(worker->thread)) {
    dbg_error("cannot cancel worker thread %d.\n", worker->id);
  }
}

struct scheduler *scheduler_new(const config_t *cfg) {
  const int workers = cfg->threads;

  struct scheduler *s = malloc(sizeof(*s));
  if (!s) {
    dbg_error("could not allocate a scheduler.\n");
    return NULL;
  }

  size_t r = HPX_CACHELINE_SIZE - sizeof(s->workers[0]) % HPX_CACHELINE_SIZE;
  size_t padded_size = sizeof(s->workers[0]) + r;
  size_t total = workers * padded_size;
  int e = posix_memalign((void**)&s->workers, HPX_CACHELINE_SIZE, total);
  if (e) {
    dbg_error("could not allocate a worker array.\n");
    scheduler_delete(s);
    return NULL;
  }

  for (int i = 0; i < workers; ++i) {
    e = worker_init(&s->workers[i], i, i, 64);
    if (e) {
      dbg_error("failed to initialize a worker.\n");
      scheduler_delete(s);
      return NULL;
    }
  }

  e = system_barrier_init(&s->barrier, NULL, workers);
  if (e) {
    dbg_error("failed to allocate the scheduler barrier.\n");
    scheduler_delete(s);
    return NULL;
  }

  sync_two_lock_queue_init(&s->yielded, NULL);

  sync_store(&s->shutdown, INT_MAX, SYNC_RELEASE);
  sync_store(&s->next_tls_id, 0, SYNC_RELEASE);
  s->n_workers    = workers;
  s->n_active_workers = workers;
  s->wf_threshold = cfg->wfthreshold;

  thread_set_stack_size(cfg->stacksize);
  log_sched("initialized a new scheduler.\n");

  // bind a worker for this thread so that we can spawn lightweight threads
  _bind_self(&s->workers[0]);
  log_sched("worker 0 ready.\n");
  return s;
}

void scheduler_delete(struct scheduler *sched) {
  if (!sched) {
    return;
  }

  // unbind this thread's worker
  self = NULL;

  system_barrier_destroy(&sched->barrier);

  if (sched->workers) {
    for (int i = 0, e = sched->n_workers; i < e; ++i) {
      worker_t *worker = scheduler_get_worker(sched, i);
      worker_fini(worker);
    }
    free(sched->workers);
  }

  sync_two_lock_queue_fini(&sched->yielded);
  free(sched);
}

worker_t *scheduler_get_worker(struct scheduler *sched, int id) {
  assert(id >= 0);
  assert(id < sched->n_workers);
  return &sched->workers[id];
}

int scheduler_startup(struct scheduler *sched, const config_t *cfg) {
  worker_t *worker = NULL;
  int status = LIBHPX_OK;

  // start all of the other worker threads
  for (int i = 1, e = sched->n_workers; i < e; ++i) {
    worker = scheduler_get_worker(sched, i);
    status = _create(worker, cfg);

    if (status != LIBHPX_OK) {
      dbg_error("could not start worker %d.\n", i);

      for (int j = 1; j < i; ++j) {
        worker = scheduler_get_worker(sched, j);
        _cancel(worker);
      }

      for (int j = 1; j < i; ++j) {
        worker = scheduler_get_worker(sched, j);
        _join(worker);
      }

      return status;
    }
  }

  status = worker_start();
  if (status != LIBHPX_OK) {
    scheduler_abort(sched);
  }

  for (int i = 1; i < sched->n_workers; ++i) {
    worker = scheduler_get_worker(sched, i);
    _join(worker);
  }

  return status;
}

void scheduler_shutdown(struct scheduler *sched, int code) {
  sync_store(&sched->shutdown, code, SYNC_RELEASE);
}

int scheduler_is_shutdown(struct scheduler *sched) {
  int shutdown = sync_load(&sched->shutdown, SYNC_ACQUIRE);
  return (shutdown != INT_MAX);
}

void scheduler_abort(struct scheduler *sched) {
  worker_t *worker = NULL;
  for (int i = 0, e = sched->n_workers; i < e; ++i) {
    worker = scheduler_get_worker(sched, i);
    _cancel(worker);
  }
}
