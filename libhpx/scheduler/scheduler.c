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

/// ----------------------------------------------------------------------------
/// @file libhpx/scheduler/schedule.c
/// ----------------------------------------------------------------------------
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <sync/barriers.h>

#include "libhpx/builtins.h"
#include "libhpx/debug.h"
#include "libhpx/network.h"
#include "libhpx/scheduler.h"
#include "thread.h"
#include "worker.h"

/// ----------------------------------------------------------------------------
/// The scheduler class.
///
/// The scheduler class represents the shared-memory state of the entire
/// scheduling process. It serves as a collection of native worker threads, and
/// a network port, and allows them to communicate with each other and the
/// network.
///
/// It is possible to have multiple scheduler instances active within the same
/// memory space---though it is unclear why we would need or want that at this
/// time---and it is theoretically possible to move workers between schedulers
/// by updating the worker's scheduler pointer and the scheduler's worker
/// table, though all of the functionality that is required to make this work is
/// not implemented.
/// ----------------------------------------------------------------------------
/// @{
struct scheduler {
  process_map_t      pmap;
  int               cores;
  struct network *network;
  int           n_workers;
  worker_t      **workers;
  sr_barrier_t   *barrier;
};


/// ----------------------------------------------------------------------------
/// A basic worker->core mapping.
///
/// Just round robin workers through the processors attached to the scheduler.
/// ----------------------------------------------------------------------------
static int _mod_pmap(scheduler_t *sched, int i) {
  return i % sched->cores;
}

scheduler_t *
scheduler_new(struct network *network, int cores, int workers, int stack_size,
              process_map_t pmap)
{
  scheduler_t *s = malloc(sizeof(*s));
  if (!s) {
    dbg_error("could not allocate a scheduler.\n");
    return NULL;
  }
  s->network   = network;
  s->cores     = cores;
  s->n_workers = workers;
  s->pmap      = (pmap) ? pmap : _mod_pmap;
  s->workers   = calloc(workers, sizeof(s->workers[0]));
  if (!s->workers) {
    dbg_error("could not allocate an array of workers.\n");
    scheduler_delete(s);
    return NULL;
  }

  s->barrier   = sr_barrier_new(workers);
  if (!s->barrier) {
    dbg_error("failed to allocate the startup barrier.\n");
    scheduler_delete(s);
    return NULL;
  }

  thread_set_stack_size(stack_size);
  dbg_log("Initialized a new scheduler.\n");
  return s;
}


void scheduler_delete(scheduler_t *sched) {
  if (!sched)
    return;

  if (sched->barrier)
    sr_barrier_delete(sched->barrier);

  if (sched->workers)
    free(sched->workers);

  free(sched);
}


int scheduler_startup(scheduler_t *sched) {
  // start all of the worker threads
  for (int i = 0, e = sched->n_workers; i < e; ++i) {
    int core = sched->pmap(sched, i);
    if ((sched->workers[i] = worker_start(i, core, sched)))
      continue;

    dbg_error("could not start worker %d.\n", i);
    for (int j = 0; j < i; ++j)
      worker_cancel(sched->workers[j]);
    return HPX_ERROR;
  }

  // return success
  return HPX_SUCCESS;
}


void scheduler_shutdown(scheduler_t *sched) {
  // signal all of the shutdown requests
  for (int i = 0; i < sched->n_workers; ++i)
    worker_shutdown(sched->workers[i]);
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


void scheduler_barrier(scheduler_t *sched, int i) {
  sr_barrier_join(sched->barrier, i);
}


int scheduler_get_n_workers(const scheduler_t *sched) {
  return sched->n_workers;
}


hpx_parcel_t *scheduler_network_recv(scheduler_t *sched) {
  return network_recv(sched->network);
}
