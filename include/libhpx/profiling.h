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
#ifdef HAVE_PAPI
#ifndef PROFILING_H
#define PROFILING_H

#include <stdint.h>
#include <papi.h>
#include <hpx/attributes.h>
#include <hpx/builtins.h>

struct config;

typedef struct {
  hpx_time_t         start_time;            // start time of first session
  size_t          session_count;            // number of recording sessions
  size_t        *counter_totals;            // totals for the counters
  int              num_counters;            // number of counters
} profile_t;

#define PROFILE_INIT {                      \
    .start_time = HPX_TIME_INIT,            \
    .session_count = 0,                     \
    .counter_totals = NULL,                 \
    .num_counters = 0                       \
    }

/// Initialize profiling. This is usually called in hpx_init().
int prof_init(struct config *cfg)
  HPX_NON_NULL(1);

/// Mark the beginning of profiling a particular event
int prof_begin();

/// Obtain the average values of the counters across all profiled sessions
/// @param      values A pointer to an array where the averages will be stored
/// @param  num_values The size of the array
int prof_get_averages(size_t *values, int num_values);

/// Reset the profiling information and stop PAPI counters if they are running
int prof_reset_counters();

/// Cleanup
int prof_fini();


// The following functions are wrappers to PAPI functionality, some of which
// perform additional management operations:

/// Begin profiling. This marks the start of an event being profiled.
/// @param      events An array of event types to be counted by PAPI
/// @param  num_events The size of the array
int papi_start_counters(int *events, int num_events);

/// Stop profiling.  This marks the end of an event being profiled.
/// @param      values A pointer to an array for storing counter values
/// @param  num_values The size of the array
int papi_stop_counters(size_t *values, int num_values);

/// Collect the current values of the profiled statistics.
/// @param      values A pointer to an array for storing counter values
/// @param  num_values The size of the array
int papi_read_counters(size_t *values, int num_values);

/// Add the counts of the previous event to the total.
/// @param      values A pointer to an array for accumulating counter values
/// @param  num_values The size of the array
int papi_accum_counters(size_t *values, int num_values);

// TODO: set up the some mechanism for users to specify which counters to use
#endif
#endif
