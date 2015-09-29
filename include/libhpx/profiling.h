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

#define SIMPLE true

// The following two macros are effectively the same, the only difference is the
// context that each should be used in. HPX_PROF_NO_RESULT is used solely as a
// return value while the other may be used as an argument to several functions.
#define HPX_PROF_NO_TAG -1
#define HPX_PROF_NO_RESULT -1

struct config;

/// The data structure representing profiling entries
/// @field        start_time Time of initialization
/// @field          end_time Time of closing
/// @field   *counter_totals Counter totals
/// @field            marked True if values have been recorded
struct profile_entry {
  hpx_time_t         start_time;
  hpx_time_t           run_time;
  int64_t       *counter_totals;
  int            last_entry;
  int            last_event;
  bool                   marked;
  bool                   paused;
  int                  eventset;
};

/// The data structure representing a profiled code event
/// @field       max_entries Maximum length of the list
/// @field       num_entries Number of entries in the list
/// @field             tally Number of occurrences of the event
/// @field              *key The name of the profiled event
/// @field          *entries The actual entries
/// @field            simple True if hardware counters don't apply
typedef struct {
  volatile int            max_entries;
  volatile int            num_entries;
  volatile int                  tally;
  char                              *key;
  volatile struct profile_entry *entries;
  volatile bool                   simple;
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
  volatile hpx_time_t         start_time;
  volatile hpx_time_t           end_time;
  volatile int              num_counters;
  volatile int                num_events;
  volatile int                max_events;
  volatile int                 *counters;
  volatile const char    **counter_names;
  volatile profile_list_t        *events;
  volatile int         current_entry;
  volatile int             current_event;
} profile_log_t;

#define PROFILE_INIT {                      \
    .start_time = HPX_TIME_INIT,            \
    .end_time = HPX_TIME_INIT,              \
    .num_counters = 0,                      \
    .num_events = 0,                        \
    .max_events = 256,                      \
    .counters = NULL,                       \
    .counter_names = NULL,                  \
    .events = NULL,                         \
    .current_entry = -1,                    \
    .current_event = -1                     \
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
int prof_get_averages(int64_t *values, char *key);

/// Return the accumulated totals of the counters
/// @param      values A pointer to an array where the totals will be stored
/// @param  num_values The size of the array
/// @param         key The key that identifies the code event
int prof_get_totals(int64_t *values, char *key);

/// Return the minimum recorded values of the counters
/// @param      values A pointer to an array where the totals will be stored
/// @param  num_values The size of the array
/// @param         key The key that identifies the code event
int prof_get_minimums(int64_t *values, char *key);

/// Return the maximum recorded values of the counters
/// @param      values A pointer to an array where the totals will be stored
/// @param  num_values The size of the array
/// @param         key The key that identifies the code event
int prof_get_maximums(int64_t *values, char *key);

/// Return the tally of event occurrences
/// @param         key The key that identifies the code event
int prof_get_tally(char *key);

/// Return the average amount of time that a code event runs for
/// @param         key The key that identifies the code event
void prof_get_average_time(char *key, hpx_time_t *avg);

/// Return the total amount of time spent on a code event
/// @param         key The key that identifies the code event
void prof_get_total_time(char *key, hpx_time_t *tot);

/// Return the minimum amount of time spent on a code event
/// @param         key The key that identifies the code event
void prof_get_min_time(char *key, hpx_time_t *min);

/// Return the maximum amount of time spent on a code event
/// @param         key The key that identifies the code event
void prof_get_max_time(char *key, hpx_time_t *max);

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
/// @param         tag A pointer that is given a unique value upon success; is
///                    used in other functions for better profiling performance
void prof_start_timing(char *key, int *tag);

/// Stop profiling.  This ends recording performance information of an event.
/// Additionally, the times are added to the running total time.
/// @param         key The key that identifies the code event
/// @param         tag Used for internal lookup; will be given a new value if
///                    the provided value is HPX_PROF_NO_TAG
int prof_stop_timing(char *key, int *tag);

/// Begin profiling. This begins recording performance information of an event.
/// @param         key The key that identifies the code event
/// @param         tag A pointer that is given a unique value upon success; is
///                    used in other functions for better profiling performance
int prof_start_hardware_counters(char *key, int *tag);

/// Stop profiling.  This ends recording performance information of an event.
/// Additionally, the counter values are added to the running total counts.
/// @param         key The key that identifies the code event
/// @param         tag Used for internal lookup; will be given a new value if
///                    the provided value is HPX_PROF_NO_TAG
int prof_stop_hardware_counters(char *key, int *tag);

/// Pause the profiling temporarily
/// @param         key The key that identifies the code event
/// @param         tag Used for internal lookup; will be given a new value if
///                    the provided value is HPX_PROF_NO_TAG
int prof_pause(char *key, int *tag);

/// Resume profiling after pausing; the tag argument should be the value
/// returned after calling the corresponding prof_pause()
/// @param         key The key that identifies the code event
/// @param         tag Used for internal lookup; will be given a new value if
///                    the provided value is HPX_PROF_NO_TAG
int prof_resume(char *key, int *tag);

#endif
