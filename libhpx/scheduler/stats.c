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

#include <stdlib.h>
#include <stdio.h>

#include "hpx/hpx.h"
#include "libsync/sync.h"
#include "libsync/locks.h"
#include "libhpx/scheduler.h"
#include "libhpx/locality.h"
#include "libhpx/stats.h"

void scheduler_stats_init(struct scheduler_stats *stats) {
  stats->spawns     = 0;
  stats->steals     = 0;
  stats->mail       = 0;
  stats->stacks     = 0;
  stats->yields     = 0;
}

struct scheduler_stats *scheduler_stats_accum(struct scheduler_stats *lhs,
                                              const struct scheduler_stats *rhs)
{
  lhs->spawns   += rhs->spawns;
  lhs->steals   += rhs->steals;
  lhs->stacks   += rhs->stacks;
  lhs->mail     += rhs->mail;
  lhs->yields   += rhs->yields;

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
  printf("spawns: %lu, ", counts->spawns);
  printf("steals: %lu, ", counts->steals);
  printf("stacks: %lu, ", counts->stacks);
  printf("mail: %lu, ", counts->mail);
  printf("\n");
  fflush(stdout);
#endif
}

