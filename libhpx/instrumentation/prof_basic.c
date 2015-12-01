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

/// Each locality maintains a single profile log
static profile_log_t _profile_log = PROFILE_INIT;

int prof_init(struct config *cfg) {
  _profile_log.counters = NULL;
  _profile_log.counter_names = NULL;
  _profile_log.num_counters = 0;
  _profile_log.events = malloc(_profile_log.max_events *
                                sizeof(profile_list_t));
  return LIBHPX_OK;
}

void prof_fini(void) {
  inst_prof_dump(_profile_log);
  for (int i = 0; i < _profile_log.num_events; i++) {
    free(_profile_log.events[i].entries);
  }
  free(_profile_log.events);
}

int prof_get_averages(int64_t *values, char *key) {
  return LIBHPX_OK;
}

int prof_get_totals(int64_t *values, char *key) {
  return LIBHPX_OK;
}

int prof_get_minimums(int64_t *values, char *key) {
  return LIBHPX_OK;
}

int prof_get_maximums(int64_t *values, char *key) {
  return LIBHPX_OK;
}

int prof_get_tally(char *key) {
  int event = profile_get_event(&_profile_log, key);
  if (event < 0) {
    return 0;
  }
  return _profile_log.events[event].tally;
}

void prof_get_average_time(char *key, hpx_time_t *avg) {
  int event = profile_get_event(&_profile_log, key);
  if (event < 0) {
    return;
  }

  uint64_t seconds, ns, average = 0;
  for (int i = 0; i < _profile_log.events[event].num_entries; i++) {
    if (_profile_log.events[event].entries[i].marked) {
      int64_t value = hpx_time_diff_ns(HPX_TIME_NULL,
                                  _profile_log.events[event].entries[i].run_time);
      if (value > 0) {
        average += value;
      }
    }
  }
  if (_profile_log.events[event].num_entries == 0) {
    dbg_error("profiler event has no entries for average.\n");
  }
  else{
    average /= _profile_log.events[event].num_entries;
  }
  seconds = average / 1e9;
  ns = average % (int64_t)1e9;

  *avg = hpx_time_construct(seconds, ns);
}

void prof_get_total_time(char *key, hpx_time_t *tot) {
  int event = profile_get_event(&_profile_log, key);
  if (event < 0) {
    return;
  }

  uint64_t seconds, ns, total = 0;
  for (int i = 0; i < _profile_log.events[event].num_entries; i++) {
    if (_profile_log.events[event].entries[i].marked) {
      total += hpx_time_diff_ns(HPX_TIME_NULL,
                                _profile_log.events[event].entries[i].run_time);
    }
  }
  seconds = total / 1e9;
  ns = total % (int64_t)1e9;

  *tot = hpx_time_construct(seconds, ns);
}

void prof_get_min_time(char *key, hpx_time_t *min) {
  int event = profile_get_event(&_profile_log, key);
  if (event < 0) {
    return;
  }

  int64_t seconds, ns, temp;
  int64_t minimum = 0;
  int start = _profile_log.events[event].num_entries;

  if (_profile_log.events[event].num_entries > 0 ) {
    for (int i = 0; i < _profile_log.events[event].num_entries; i++) {
      if (_profile_log.events[event].entries[i].marked &&
         0 < hpx_time_diff_ns(HPX_TIME_NULL, _profile_log.events[event].entries[i].run_time)) {
        minimum = hpx_time_diff_ns(HPX_TIME_NULL,
                              _profile_log.events[event].entries[i].run_time);
        start = i+1;
        break;
      }
    }
  }
  for (int i = start; i < _profile_log.events[event].num_entries; i++) {
    if (_profile_log.events[event].entries[i].marked) {
      temp = hpx_time_diff_ns(HPX_TIME_NULL,
                              _profile_log.events[event].entries[i].run_time);
      if (temp < minimum && 0 < temp) {
        minimum = temp;
      }
    }
  }
  seconds = minimum / 1e9;
  ns = minimum % (int64_t)1e9;

  *min = hpx_time_construct(seconds, ns);
}

void prof_get_max_time(char *key, hpx_time_t *max) {
  int event = profile_get_event(&_profile_log, key);
  if (event < 0) {
    return;
  }

  int64_t seconds, ns, temp;
  int64_t maximum = 0;
  int start = _profile_log.events[event].num_entries;

  if (_profile_log.events[event].num_entries > 0 ) {
    for (int i = 0; i < _profile_log.events[event].num_entries; i++) {
      if (_profile_log.events[event].entries[i].marked) {
        maximum =
             hpx_time_diff_ns(HPX_TIME_NULL,
                              _profile_log.events[event].entries[0].run_time);
        start = i+1;
        break;
      }
    }
  }
  for (int i = start; i < _profile_log.events[event].num_entries; i++) {
    if (_profile_log.events[event].entries[i].marked) {
      temp = hpx_time_diff_ns(HPX_TIME_NULL,
                              _profile_log.events[event].entries[i].run_time);
      if (temp > maximum) {
        maximum = temp;
      }
    }
  }
  seconds = maximum / 1e9;
  ns = maximum % (int64_t)1e9;

  *max = hpx_time_construct(seconds, ns);
}

int prof_get_num_counters(void) {
  return _profile_log.num_counters;
}

void prof_increment_tally(char *key) {
  int event = profile_get_event(&_profile_log, key);
  if (event < 0) {
    event = profile_new_event(&_profile_log, key, true, 0);
  }

  _profile_log.events[event].tally++;
}

void prof_start_timing(char *key, int *tag) {
  hpx_time_t now = hpx_time_now();
  int event = profile_get_event(&_profile_log, key);
  if (event < 0) {
    event = profile_new_event(&_profile_log, key, true, 0);
  }
  dbg_assert(event >= 0);

  // interrupt current timing
  if (_profile_log.current_event >= 0 &&
     !_profile_log.events[_profile_log.current_event].entries[
                          _profile_log.current_entry].marked &&
     !_profile_log.events[_profile_log.current_event].entries[
                          _profile_log.current_entry].paused) {
    hpx_time_t dur;
    hpx_time_diff(_profile_log.events[_profile_log.current_event].entries[
                  _profile_log.current_entry].start_time, now, &dur);
    _profile_log.events[_profile_log.current_event].entries[
                        _profile_log.current_entry].run_time =
             hpx_time_add(_profile_log.events[_profile_log.current_event].entries[
                        _profile_log.current_entry].run_time, dur);
  }

  int index = profile_new_entry(&_profile_log, event);
  _profile_log.events[event].entries[index].last_entry = _profile_log.current_entry;
  _profile_log.events[event].entries[index].last_event = _profile_log.current_event;
  _profile_log.current_entry = index;
  _profile_log.current_event = event;
  _profile_log.events[event].entries[index].start_time = hpx_time_now();
  *tag = index;
}

int prof_stop_timing(char *key, int *tag) {
  hpx_time_t end = hpx_time_now();
  int event = profile_get_event(&_profile_log, key);
  if (event < 0) {
    return 0;
  }

  if (*tag == HPX_PROF_NO_TAG) {
    for (int i = _profile_log.events[event].num_entries - 1; i >= 0; i--) {
      if (!_profile_log.events[event].entries[i].marked) {
        *tag = i;
        break;
      }
    }
  }
  if (*tag == HPX_PROF_NO_TAG) {
    return event;
  }

  if (!_profile_log.events[event].entries[*tag].paused) {
    hpx_time_t dur;
    hpx_time_diff(_profile_log.events[event].entries[*tag].start_time, end, &dur);
    _profile_log.events[event].entries[*tag].run_time =
        hpx_time_add(_profile_log.events[event].entries[*tag].run_time, dur);
  }
  _profile_log.events[event].entries[*tag].marked = true;

  // if another event/entry was being measured prior to switching to the current
  // event/entry, then pick up where we left off
  if (_profile_log.events[event].entries[*tag].last_event >= 0) {
    _profile_log.current_entry = _profile_log.events[event].entries[*tag].last_entry;
    _profile_log.current_event = _profile_log.events[event].entries[*tag].last_event;
    if (!_profile_log.events[event].entries[_profile_log.current_entry].paused) {
      _profile_log.events[_profile_log.current_event].entries
                         [_profile_log.current_entry].start_time
                          = hpx_time_now();
    }
  }
  return event;
}

int prof_start_hardware_counters(char *key, int *tag) {
  prof_start_timing(key, tag);
  return LIBHPX_OK;
}

int prof_stop_hardware_counters(char *key, int *tag) {
  prof_stop_timing(key, tag);
  return LIBHPX_OK;
}

int prof_pause(char *key, int *tag) {
  hpx_time_t end = hpx_time_now();
  int event = profile_get_event(&_profile_log, key);
  if (event < 0) {
    return LIBHPX_OK;
  }

  if (*tag == HPX_PROF_NO_TAG) {
    for (int i = _profile_log.events[event].num_entries - 1; i >= 0; i--) {
      if (!_profile_log.events[event].entries[i].marked &&
         !_profile_log.events[event].entries[i].paused) {
        *tag = i;
        break;
      }
    }
  }
  if (*tag == HPX_PROF_NO_TAG ||
     _profile_log.events[event].entries[*tag].marked ||
     _profile_log.events[event].entries[*tag].paused) {
    return LIBHPX_OK;
  }

  // first store timing information
  hpx_time_t dur;
  hpx_time_diff(_profile_log.events[event].entries[*tag].start_time, end, &dur);
  _profile_log.events[event].entries[*tag].run_time =
      hpx_time_add(_profile_log.events[event].entries[*tag].run_time, dur);

  _profile_log.events[event].entries[*tag].paused = true;
  return LIBHPX_OK;
}

int prof_resume(char *key, int *tag) {
  int event = profile_get_event(&_profile_log, key);
  if (event < 0) {
    return LIBHPX_OK;
  }

  if (*tag == HPX_PROF_NO_TAG) {
    for (int i = _profile_log.events[event].num_entries - 1; i >= 0; i--) {
      if (!_profile_log.events[event].entries[i].marked &&
         _profile_log.events[event].entries[i].paused) {
        *tag = i;
        break;
      }
    }
  }
  if (*tag == HPX_PROF_NO_TAG) {
    return LIBHPX_OK;
  }

  _profile_log.events[event].entries[*tag].paused = false;
  _profile_log.events[event].entries[*tag].start_time = hpx_time_now();
  return LIBHPX_OK;
}

