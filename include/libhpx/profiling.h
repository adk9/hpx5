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
  hpx_time_t         start_time;            // start time of recording session
  hpx_time_t           end_time;            // end time of recording session
  size_t                  tally;            // number of events
  long long     *counter_totals;            // totals for the counters
  int                 *counters;            // the counters used
  int              num_counters;            // number of counters
  bool             papi_running;            // true if PAPI is recording
} profile_t;

#define PROFILE_INIT {                      \
    .start_time = HPX_TIME_INIT,            \
    .end_time = HPX_TIME_INIT,              \
    .tally = 0,                             \
    .counter_totals = NULL,                 \
    .counters = NULL,                       \
    .num_counters = 0,                      \
    .papi_running = false                   \
    }

/// Initialize profiling. This is usually called in hpx_init().
int prof_init(struct config *cfg)
  HPX_NON_NULL(1);

/// Mark the beginning of profiling a type of event.
/// The event may occur several times between prof_begin() and prof_end(),
/// and profiling of the event occurrences should be done using 
/// papi_start_counters() and papi_stop_counters(), or prof_tally_mark() if
/// details of the events performance are not required.
void prof_begin();

/// Mark the end of profiling a type of event.  This will also stop any running
/// PAPI counters.  Note that if PAPI counters are not currently running any 
/// arguments passed in will be ignored and the end time will just be recorded.
/// @param      values A pointer to an array where the values will be stored
/// @param  num_values The size of the array
int prof_end(long long *values, int num_values);

/// Mark the occurrence of an event
void prof_tally_mark();

/// Obtain the average values of the counters across all profiled sessions
/// @param      values A pointer to an array for storing counter values
/// @param  num_values The size of the array
int prof_get_averages(long long *values, int num_values);

/// Return the accumulated totals of the counters
/// @param      values A pointer to an array where the totals will be stored
/// @param  num_values The size of the array
int prof_get_totals(long long *values, int num_values);

/// Return the tally of event occurrences
size_t prof_get_tally();

/// Return the number of counters being used
int prof_get_num_counters();

/// Return the time since the recording session began, or if the session has
/// ended, the duration of the recording session
hpx_time_t prof_get_duration();

/// Reset the profiling information and stop PAPI counters if they are running
int prof_reset();

/// Cleanup
int prof_fini();


/// Begin profiling. This begins recording performance information of an event.
int prof_start_papi_counters();

/// Stop profiling.  This ends recording performance information of an event.
/// Additionally, the counter values are added to the running total counts.
/// @param      values A pointer to an array for storing counter values
/// @param  num_values The size of the array
int prof_stop_papi_counters(long long *values, int num_values);

/// Collect the current values of the profiled statistics.  This will reset the
/// counters but leave them running. Additionally, the counter values are added 
/// to the running total counts.
/// @param      values A pointer to an array for storing counter values
/// @param  num_values The size of the array
int prof_read_papi_counters(long long *values, int num_values);

/// Add the counts of the previous event to the passed in array.  This will 
/// reset the counters but leave them running. Additionally, the counter values 
/// are added to the running total counts.
/// @param      values A pointer to an array for accumulating counter values
/// @param  num_values The size of the array
int prof_accum_papi_counters(long long *values, int num_values);

#endif
#endif
