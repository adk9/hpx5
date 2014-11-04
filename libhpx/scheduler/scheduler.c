// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/// @file libhpx/scheduler/schedule.c
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#include "hpx/builtins.h"
#include "libsync/sync.h"
#include "libsync/barriers.h"
#include "libhpx/debug.h"
#include "libhpx/scheduler.h"
#include "thread.h"
#include "worker.h"


scheduler_t *
scheduler_new(int cores, int workers, int stack_size, unsigned int backoff_max,
              bool stats)
{
  scheduler_t *s = malloc(sizeof(*s));
  if (!s) {
    dbg_error("scheduler: could not allocate a scheduler.\n");
    return NULL;
  }

  sync_store(&s->next_id, 0, SYNC_RELEASE);
  sync_store(&s->next_tls_id, 0, SYNC_RELEASE);

  s->cores       = cores;
  s->n_workers   = workers;
  s->backoff_max = backoff_max;
  s->workers     = calloc(workers, sizeof(s->workers[0]));
  if (!s->workers) {
    dbg_error("scheduler: could not allocate an array of workers.\n");
    scheduler_delete(s);
    return NULL;
  }

  s->barrier = sr_barrier_new(workers);
  if (!s->barrier) {
    dbg_error("scheduler: failed to allocate the startup barrier.\n");
    scheduler_delete(s);
    return NULL;
  }

  s->stats = (scheduler_stats_t) SCHEDULER_STATS_INIT;

  YIELD_QUEUE_INIT(&s->yielded, NULL);

  thread_set_stack_size(stack_size);
  dbg_log_sched("initialized a new scheduler.\n");
  return s;
}

void scheduler_delete(scheduler_t *sched) {
  if (!sched)
    return;

#ifdef HPX_PROFILE_STACKS
  printf("High water mark for stacks was %lu\n", sched->stats.max_stacks);
#endif

  if (sched->barrier)
    sync_barrier_delete(sched->barrier);

  if (sched->workers)
    free(sched->workers);

  YIELD_QUEUE_FINI(&sched->yielded);

  free(sched);
}

int scheduler_startup(scheduler_t *sched) {
  // start all of the other worker threads
  for (int i = 0, e = sched->n_workers - 1; i < e; ++i) {
    if (worker_start(sched) == 0)
      continue;

    dbg_error("scheduler: could not start worker %d.\n", i);

    for (int j = 0; j < i; ++j)
      worker_cancel(sched->workers[j]);

    for (int j = 0; j < i; ++j)
      worker_join(sched->workers[j]);

    return HPX_ERROR;
  }

  worker_run(sched);
  scheduler_join(sched);

  return HPX_SUCCESS;
}


void scheduler_shutdown(scheduler_t *sched) {
  // signal all of the shutdown requests
  for (int i = 0; i < sched->n_workers; ++i)
    worker_shutdown(sched->workers[i]);
}


void scheduler_join(scheduler_t *sched) {
  int me = hpx_get_my_thread_id();
  // wait for the workers to shutdown
  for (int i = 0; i < sched->n_workers; ++i) {
    if (i == me)
      continue;
    worker_join(sched->workers[i]);
  }
}


/// Abort the scheduler.
///
/// This will wait for all of the children to cancel, but won't do any cleanup
/// since we have no way to know if they are in async-safe functions that we
/// need during cleanup (e.g., holding the malloc lock).
void scheduler_abort(scheduler_t *sched) {
  for (int i = 0, e = sched->n_workers; i < e; ++i)
    worker_cancel(sched->workers[i]);
}

