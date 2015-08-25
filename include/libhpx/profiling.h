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

#ifndef PROFILING_H
#define PROFILING_H

#include <stdint.h>
#include <hpx/attributes.h>
#include <hpx/builtins.h>

struct config;

/// The data structure representing profiling entries
/// @field        start_time Time of initialization
/// @field          end_time Time of closing
/// @field   *counter_totals Counter totals
/// @field            marked True if values have been recorded
struct profile_entry {
  hpx_time_t         start_time;
  hpx_time_t           end_time;
  long long     *counter_totals;
  bool                   marked;
};

/// The data structure representing a profiled code event
/// @field       max_entries Maximum length of the list
/// @field       num_entries Number of entries in the list
/// @field             tally Number of occurrences of the event
/// @field              *key The name of the profiled event
/// @field          *entries The actual entries
/// @field            simple True if hardware counters don't apply
typedef struct {
  size_t            max_entries;
  size_t            num_entries;
  size_t                  tally;
  const char               *key;
  struct profile_entry *entries;
  bool                   simple;
} profile_list_t;

/// The data structure for storing profiling entries
/// @field        start_time Time of initialization
/// @field          end_time Time of closing
/// @field         cur_depth Current relative stack frame to other entries
/// @field      num_counters Number of counters utilized
/// @field       num_entries Number of code events profiled
/// @field       max_entries Maximum number of code events profilable
/// @field         *counters The ids of the counters used
/// @field   **counter_names The string names of the counters
/// @field          *entries The actual entries
typedef struct {
  hpx_time_t         start_time;
  hpx_time_t           end_time;
  size_t           num_counters;
  size_t            num_entries;
  size_t            max_entries;
  int                 *counters;
  const char    **counter_names;
  profile_list_t       *entries;
} profile_log_t;

#define PROFILE_INIT {                      \
    .start_time = HPX_TIME_INIT,            \
    .end_time = HPX_TIME_INIT,              \
    .num_counters = 0,                      \
    .num_entries = 0,                     \
    .max_entries = 2,                     \
    .counters = NULL,                       \
    .counter_names = NULL,                  \
    .entries = NULL,                        \
    }

/// Initialize profiling. This is usually called in hpx_init().
void prof_init(struct config *cfg)
  HPX_NON_NULL(1);

/// Cleanup
int prof_fini();

/// Obtain the average values of the counters across all profiled sessions
/// @param      values A pointer to an array for storing counter values
/// @param  num_values The size of the array
/// @param         key The key that identifies the code event
int prof_get_averages(long long *values, int num_values, char *key);

/// Return the accumulated totals of the counters
/// @param      values A pointer to an array where the totals will be stored
/// @param  num_values The size of the array
/// @param         key The key that identifies the code event
int prof_get_totals(long long *values, int num_values, char *key);

/// Return the tally of event occurrences
/// @param         key The key that identifies the code event
size_t prof_get_tally(char *key);

/// Return the average amount of time that a code event runs for
/// @param         key The key that identifies the code event
void prof_get_average_time(char *key, hpx_time_t *avg);

/// Return the total amount of time spent on a code event
/// @param         key The key that identifies the code event
void prof_get_total_time(char *key, hpx_time_t *tot);

/// Return the number of counters being used
int prof_get_num_counters();

/// Return the time since the recording session began, or if the session has
/// ended, the duration of the recording session
hpx_time_t prof_get_duration();

/// Mark the occurrence of an event
/// @param         key The key that identifies the code event
void prof_increment_tally(char *key);

/// Begin profiling. This begins recording performance information of an event.
/// @param         key The key that identifies the code event
void prof_start_timing(char *key);

/// Stop profiling.  This ends recording performance information of an event.
/// Additionally, the times are added to the running total time.
/// @param         key The key that identifies the code event
void prof_stop_timing(char *key);

/// Begin profiling. This begins recording performance information of an event.
/// @param         key The key that identifies the code event
int prof_start_hardware_counters(char *key);

/// Stop profiling.  This ends recording performance information of an event.
/// Additionally, the counter values are added to the running total counts.
/// @param         key The key that identifies the code event
int prof_stop_hardware_counters(char *key);

#endif
