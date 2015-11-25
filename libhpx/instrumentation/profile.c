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
# include "config.h"
#endif

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <hpx/hpx.h>
#include <libhpx/config.h>
#include <libhpx/debug.h>
#include <libhpx/instrumentation.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/profiling.h>
#include <libsync/sync.h>

int profile_new_list(profile_log_t *log, char *key, bool simple) {
  if (!log) {
    return LIBHPX_ERROR;
  }
              
  if (log->num_events == log->max_events) {
    log->max_events *= 2;
    size_t bytes = log->max_events * sizeof(profile_list_t);
    log->events = realloc(log->events, bytes);
    dbg_assert(log->events);
  }
  int index = log->num_events++;
  profile_list_t *list = &log->events[index];
  list->entries = malloc(log->max_events * sizeof(profile_entry_t));
  dbg_assert(list->entries);
  list->tally = 0;
  list->num_entries = 0;
  list->max_entries = log->max_events;
  list->key = key;
  list->simple = simple;
  return LIBHPX_OK;
}

int profile_new_entry(profile_log_t *log, int event, int eventset) {
  profile_list_t *list = &log->events[event];
  dbg_assert(list->max_entries > 0);
  if (list->num_entries == list->max_entries) {
    list->max_entries *= 2;
    size_t bytes = list->max_entries * sizeof(profile_entry_t);
    list->entries = realloc(list->entries, bytes);
    dbg_assert(list->entries);
  }

  int index = list->num_entries++;
  list->tally++;
  list->entries[index].run_time = HPX_TIME_NULL;
  list->entries[index].marked = false;
  list->entries[index].paused = false;
  list->entries[index].eventset = eventset;

  list->entries[index].counter_totals = NULL;
  if (list->simple) {
    list->entries[index].counter_totals = NULL;
  } else {
    list->entries[index].counter_totals =
        malloc(log->num_counters * sizeof(int64_t));
    for (int i = 0; i < log->num_counters; ++i) {
      list->entries[index].counter_totals[i] = HPX_PROF_NO_RESULT;
    }
  }
  return index;
}

// Returns index of matching key/creates new entry if index doesn't
// exist
int profile_get_event(profile_log_t *log, char *key) {
  for (int i = 0; i < log->num_events; i++) {
    if (strcmp(key, log->events[i].key) == 0) {
      return i;
    }
  }
  return HPX_PROF_NO_RESULT;
}

