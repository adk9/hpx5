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
#include "config.h"
#endif
#ifdef HAVE_APEX
#include "apex.h"
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

void scheduler_stats_init(struct scheduler_stats *stats) {
  stats->spins      = 0;
  stats->spawns     = 0;
  stats->steals     = 0;
  stats->mail       = 0;
  stats->started    = 0;
  stats->finished   = 0;
  stats->progress   = 0;
  stats->backoffs   = 0;
  stats->max_stacks = 0;
  stats->stacks     = 0;
  stats->yields     = 0;
  stats->backoff  = 0.0;
}

struct scheduler_stats *scheduler_stats_accum(struct scheduler_stats *lhs,
                                              const struct scheduler_stats *rhs)
{
  lhs->spins    += rhs->spins;
  lhs->spawns   += rhs->spawns;
  lhs->steals   += rhs->steals;
  lhs->stacks   += rhs->stacks;
  lhs->mail     += rhs->mail;
  lhs->started  += rhs->started;
  lhs->finished += rhs->finished;
  lhs->progress += rhs->progress;
  lhs->backoffs += rhs->backoffs;
  lhs->yields   += rhs->yields;
  lhs->backoff  += rhs->backoff;

  return lhs;
}


void scheduler_stats_print(const char *id, const struct scheduler_stats *counts)
{
#ifdef ENABLE_PROFILING
  if (!here || !counts)
    return;

  printf("node %d, ", here->rank);
  printf("thread %s, ", id);
  printf("yields: %lu, ", counts->yields);
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
#endif
}

#ifdef HAVE_APEX
void scheduler_save_apex_stats(const scheduler_stats_t *counts) {
  apex_sample_value("spins", (double)counts->spins);
  apex_sample_value("yields", (double)counts->yields);
  apex_sample_value("spawns", (double)counts->spawns);
  apex_sample_value("steals", (double)counts->steals);
  apex_sample_value("stacks", (double)counts->stacks);
  apex_sample_value("mail", (double)counts->mail);
  apex_sample_value("started", (double)counts->started);
  apex_sample_value("finished", (double)counts->finished);
  apex_sample_value("progress", (double)counts->progress);
  apex_sample_value("backoffs", (double)counts->backoffs);
  apex_sample_value("backoff (ms)", (double)counts->backoff);
}
#endif


