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

/// @file libhpx/scheduler/schedule.c
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <hpx/builtins.h>
#include <libhpx/config.h>
#include <libhpx/debug.h>
#include <libhpx/libhpx.h>
#include <libhpx/scheduler.h>
#include "thread.h"

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
    e = worker_init(&s->workers[i], s, i, i, 64);
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
  s->wf_threshold = cfg->wfthreshold;
  scheduler_stats_init(&s->stats);

  thread_set_stack_size(cfg->stacksize);
  log_sched("initialized a new scheduler.\n");

  // bind a worker for this thread so that we can spawn lightweight threads
  worker_bind_self(&s->workers[0]);
  log_sched("worker 0 ready.\n");
  return s;
}

void scheduler_delete(struct scheduler *sched) {
  if (!sched) {
    return;
  }

  system_barrier_destroy(&sched->barrier);

  if (sched->workers) {
    for (int i = 0, e = sched->n_workers; i < e; ++i) {
      struct worker *worker = scheduler_get_worker(sched, i);
      worker_fini(worker);
    }
    free(sched->workers);
  }

  sync_two_lock_queue_fini(&sched->yielded);

  free(sched);
}


void scheduler_dump_stats(struct scheduler *sched) {
  char id[16] = {0};
  for (int i = 0, e = sched->n_workers; i < e; ++i) {
    struct worker *w = scheduler_get_worker(sched, i);
    snprintf(id, 16, "%d", w->id);
    scheduler_stats_print(id, &w->stats);
    scheduler_stats_accum(&sched->stats, &w->stats);
  }

  scheduler_stats_print("<totals>", &sched->stats);
}


struct worker *scheduler_get_worker(struct scheduler *sched, int id) {
  assert(id >= 0);
  assert(id < sched->n_workers);
  return &sched->workers[id];
}


int scheduler_startup(struct scheduler *sched, const config_t *cfg) {
  struct worker *worker = NULL;
  int status = LIBHPX_OK;

  // start all of the other worker threads
  for (int i = 1, e = sched->n_workers; i < e; ++i) {
    worker = scheduler_get_worker(sched, i);
    status = worker_create(worker, cfg);

    if (status != LIBHPX_OK) {
      dbg_error("could not start worker %d.\n", i);

      for (int j = 1; j < i; ++j) {
        worker = scheduler_get_worker(sched, j);
        worker_cancel(worker);
      }

      for (int j = 1; j < i; ++j) {
        worker = scheduler_get_worker(sched, j);
        worker_join(worker);
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
    worker_join(worker);
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
  struct worker *worker = NULL;
  for (int i = 0, e = sched->n_workers; i < e; ++i) {
    worker = scheduler_get_worker(sched, i);
    worker_cancel(worker);
  }
}

scheduler_stats_t *scheduler_get_stats(struct scheduler *sched) {
  if (sched) {
    return &sched->stats;
  }
  else {
    return NULL;
  }
}

int scheduler_nop_handler(void) {
  return HPX_SUCCESS;
}
LIBHPX_ACTION(HPX_INTERRUPT, 0, scheduler_nop, scheduler_nop_handler);
