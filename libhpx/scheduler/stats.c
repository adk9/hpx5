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
/// @file libhpx/scheduler/statistics.c
/// ----------------------------------------------------------------------------
#include <stdlib.h>
#include <stdio.h>

#include "hpx/hpx.h"

#include "libsync/sync.h"
#include "libsync/locks.h"
#include "libhpx/scheduler.h"
#include "libhpx/locality.h"
#include "libhpx/stats.h"


void scheduler_accum_stats(struct scheduler *sched, const scheduler_stats_t *worker) {
#ifdef ENABLE_PROFILING
  scheduler_stats_t *total = &sched->stats;
  static tatas_lock_t lock = SYNC_TATAS_LOCK_INIT;
  sync_tatas_acquire(&lock);
  total->spins    += worker->spins;
  total->spawns   += worker->spawns;
  total->steals   += worker->steals;
  total->stacks   += worker->stacks;
  total->mail     += worker->mail;
  total->started  += worker->started;
  total->finished += worker->finished;
  total->progress += worker->progress;
  total->backoffs += worker->backoffs;
  total->backoff  += worker->backoff;
  sync_tatas_release(&lock);
#endif
}


void scheduler_print_stats(const char *id, const scheduler_stats_t *counts) {
#ifdef ENABLE_PROFILING
  static tatas_lock_t lock = SYNC_TATAS_LOCK_INIT;
  sync_tatas_acquire(&lock);
  printf("node %d, ", here->rank);
  printf("thread %s, ", id);
  printf("spins: %lu, ", counts->spins);
  printf("spawns: %lu, ", counts->spawns);
  printf("steals: %lu, ", counts->steals);
  printf("stacks: %lu, ", counts->stacks);
  printf("mail: %lu, ", counts->mail);
  printf("started: %lu, ", counts->started);
  printf("finished: %lu, ", counts->finished);
  printf("progress: %lu, ", counts->progress);
  printf("backoffs: %lu (%.1fms)", counts->backoffs, counts->backoff);
  printf("\n");
  fflush(stdout);
  sync_tatas_release(&lock);
#endif
}


scheduler_stats_t *scheduler_get_stats(void) {
  return &here->sched->stats;
}

