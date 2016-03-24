// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2016, Trustees of Indiana University,
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

#ifndef __cplusplus
# include <stdbool.h>
#endif

#include <stdint.h>
#include <hpx/attributes.h>
#include <hpx/builtins.h>
#include <hpx/time.h>

#define SIMPLE true

// Indicates a profile event with no tag.
#define HPX_PROF_NO_TAG -1

struct config;

/// The data structure representing profiling entries
typedef struct {
  hpx_time_t   start_time; //!< Time of initialization
  hpx_time_t     ref_time; //!< Time of resumption of recording session
  hpx_time_t     run_time; //!< Total time spent in this entry
  int64_t *counter_totals; //!< Counter totals
  double         user_val; //!< Stores the value of a user-defined metric
  int          last_entry; //!< The previous entry being recorded
  int          last_event; //!< The previous event being recorded
  bool             marked; //!< True if values have been recorded
  bool             paused; //!< True if recording has been paused
} profile_entry_t;

/// The data structure representing a profiled code event
typedef struct {
  int          max_entries; //!< Maximum length of the list
  int          num_entries; //!< Number of entries in the list
  char                *key; //!< The name of the profiled event
  profile_entry_t *entries; //!< The actual entries
  bool              simple; //!< True if hardware counters don't apply
  int             eventset; //!< The eventset that will be used for this event
} profile_list_t;

/// The data structure for storing profiling entries
typedef struct profile_log {
  int           num_counters; //!< Number of counters utilized
  int             num_events; //!< Number of code events profiled
  int             max_events; //!< Maximum number of code events profilable
  int              *counters; //!< The ids of the counters used
  profile_list_t     *events; //!< The actual profiled events
  int          current_entry; //!< The current entry
  int          current_event; //!< The current code event
} profile_log_t;

/// Add a new entry to the profile list in the profile log.
/// @param             event The event id of the new entry being added
/// @returns                 Index of the new entry.
int profile_new_entry(int event);

/// Get the event corresponding to the event key @p key in the profile
/// log.
/// @param               key The key of the event we are getting
int profile_get_event(char *key);

/// Create a new profile event list in the profile log.
/// @param               key The key of the event we are creating
/// @param            simple True if hardware counters don't apply
/// @param          eventset The eventset of the new event entry
int profile_new_event(char *key, bool simple, int eventset);

/// Initialize profiling. This is usually called in hpx_init().
int prof_init(const struct config *cfg)
  HPX_NON_NULL(1);

/// Cleanup
void prof_fini(void);

/// Obtain the average values of the counters across all profiled sessions
/// @param      values A pointer to an array for storing counter values
/// @param         key The key that identifies the code event
int prof_get_averages(int64_t *values, char *key);

/// Return the accumulated totals of the counters
/// @param      values A pointer to an array where the totals will be stored
/// @param         key The key that identifies the code event
int prof_get_totals(int64_t *values, char *key);

/// Return the minimum recorded values of the counters
/// @param      values A pointer to an array where the totals will be stored
/// @param         key The key that identifies the code event
int prof_get_minimums(int64_t *values, char *key);

/// Return the maximum recorded values of the counters
/// @param      values A pointer to an array where the totals will be stored
/// @param         key The key that identifies the code event
int prof_get_maximums(int64_t *values, char *key);

/// Return the total of the user-defined metric
/// @param         key The key that identifies the code event
double prof_get_user_total(char *key);

/// Return the number of event occurrences
/// @param         key The key that identifies the code event
int prof_get_event_count(char *key);

/// Return the average amount of time that a code event runs for
/// @param         key The key that identifies the code event
/// @param         avg The average duration of the code event
void prof_get_average_time(char *key, hpx_time_t *avg);

/// Return the total amount of time spent on a code event
/// @param         key The key that identifies the code event
/// @param         tot The total time spent in the code event
void prof_get_total_time(char *key, hpx_time_t *tot);

/// Return the minimum amount of time spent on a code event
/// @param         key The key that identifies the code event
/// @param         min The min recorded duration of the code event
void prof_get_min_time(char *key, hpx_time_t *min);

/// Return the maximum amount of time spent on a code event
/// @param         key The key that identifies the code event
/// @param         max The max recorded duration of the code event
void prof_get_max_time(char *key, hpx_time_t *max);

/// Return the number of counters being used
int prof_get_num_counters(void);

/// Add to the user defined metric's total
/// @param         key The key that identifies the code event
/// @param      amount The amount to add to the total
void prof_record_user_val(char *key, double amount);

/// Mark the occurrence of an event
/// @param         key The key that identifies the code event
void prof_mark(char *key);

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
