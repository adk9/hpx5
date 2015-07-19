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
#ifndef LIBHPX_STATS_H
#define LIBHPX_STATS_H

#include "hpx/hpx.h"

/// @file libhpx/scheduler/stats.h
/// @brief The libhpx stats definitions.

#ifdef ENABLE_PROFILING
#define profile_ctr(e) e
#else
#define profile_ctr(e)
#endif

/// Forward declarations
/// @{
struct scheduler;
/// @}

/// HPX scheduler statistics.
///
/// These are statistics mostly related to the scheduler that are
/// updated during program execution. Local (per-worker) statistics
/// are stored in each worker's TLS and updated by each worker
/// separately. Global (per-scheduler) statistics are accumulated
/// after each worker shuts down.
typedef struct scheduler_stats {
  unsigned long     spawns;
  unsigned long     steals;
  unsigned long       mail;
  unsigned long     stacks;
  unsigned long     yields;
} scheduler_stats_t;

#define SCHEDULER_STATS_INIT {   \
    .spawns     = 0,             \
    .steals     = 0,             \
    .mail       = 0,             \
    .stacks     = 0,             \
    .yields     = 0,             \
  }

/// Initialize a scheduler statistics structure.
void scheduler_stats_init(struct scheduler_stats *stats)
  HPX_NON_NULL(1);

/// Accumulate scheduler statistics.
///
/// This is not synchronized.
///
/// @param          lhs The counts we're accumulating into.
/// @param          rhs The counts we're trying to add to.
///
/// @returns            lhs
struct scheduler_stats *scheduler_stats_accum(struct scheduler_stats *lhs,
                                              const struct scheduler_stats *rhs)
  HPX_NON_NULL(1, 2);

/// Print scheduler stats.
void scheduler_stats_print(const char *id, const struct scheduler_stats *stats)
  HPX_NON_NULL(2);

/// Get scheduler statistics.
struct scheduler_stats *scheduler_get_stats(struct scheduler *sched);

/// Get schedulers statistics for the current thread.
struct scheduler_stats *thread_get_stats(void);

#endif // LIBHPX_STATS_H
