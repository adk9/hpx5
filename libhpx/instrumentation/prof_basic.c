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

#define TIME_NULL hpx_time_construct(0, 0)

/// Each locality maintains a single profile log
static profile_log_t _profile_log = PROFILE_INIT;

static void _prof_begin(){
  _profile_log.start_time = hpx_time_now();
}

static void _prof_end(){
  _profile_log.end_time = hpx_time_now();
}

static void _create_new_list(char *key, bool simple) {
  if (_profile_log.num_events == _profile_log.max_events) {
    _profile_log.max_events *= 2;
    size_t bytes = _profile_log.max_events * sizeof(profile_list_t);
    _profile_log.events = realloc(_profile_log.events, bytes);
    dbg_assert(_profile_log.events);
  }
  int index = _profile_log.num_events++;
  profile_list_t *list = &_profile_log.events[index];
  list->entries = malloc(_profile_log.max_events * sizeof(profile_entry_t));
  dbg_assert(list->entries);
  list->tally = 0;
  list->num_entries = 0;
  list->max_entries = _profile_log.max_events;
  list->key = key;
  list->simple = simple;
}

static int _create_new_entry(int event, bool simple) {
  profile_list_t *list = &_profile_log.events[event];
  dbg_assert(list->max_entries > 0);
  if (list->num_entries == list->max_entries) {
    list->max_entries *= 2;
    size_t bytes = list->max_entries * sizeof(profile_entry_t);
    list->entries = realloc(list->entries, bytes);
    dbg_assert(list->entries);
  }

  int index = list->num_entries++;
  list->tally++;
  list->entries[index].counter_totals = NULL;
  list->entries[index].run_time = TIME_NULL;
  list->entries[index].marked = false;
  list->entries[index].paused = false;
  return index;
}

//returns index of matching key/creates new entry if index doesn't exist
static int _get_event_num(char *key){
  for(int i = 0; i < _profile_log.num_events; i++){
    if(strcmp(key, _profile_log.events[i].key) == 0){
      return i;
    }
  }
  return HPX_PROF_NO_RESULT;
}

static hpx_time_t _add_times(hpx_time_t time1, hpx_time_t time2){
  int64_t seconds, ns, total;
  total = hpx_time_diff_ns(TIME_NULL, time1) + hpx_time_diff_ns(TIME_NULL, time2);
  seconds = total / 1e9;
  ns = total % (int64_t)1e9;
  return hpx_time_construct(seconds, ns);
}

void prof_init(struct config *cfg){
  _profile_log.counters = NULL;
  _profile_log.counter_names = NULL;
  _profile_log.num_counters = 0;
  _profile_log.events = malloc(_profile_log.max_events *
                                sizeof(profile_list_t));

  _prof_begin();
}

int prof_fini(){
  _prof_end();
  inst_prof_dump(_profile_log);
  for(int i = 0; i < _profile_log.num_events; i++){
    free((void *)_profile_log.events[i].entries);
  }
  free((void *)_profile_log.events);
  return LIBHPX_OK;
}

int prof_get_averages(int64_t *values, char *key){
  return LIBHPX_OK;
}

int prof_get_totals(int64_t *values, char *key){
  return LIBHPX_OK;
}

int prof_get_minimums(int64_t *values, char *key){
  return LIBHPX_OK;
}

int prof_get_maximums(int64_t *values, char *key){
  return LIBHPX_OK;
}

int prof_get_tally(char *key){
  int event = _get_event_num(key);
  if(event == HPX_PROF_NO_RESULT){
    return 0;
  }
  return _profile_log.events[event].tally;
}

void prof_get_average_time(char *key, hpx_time_t *avg){
  int event = _get_event_num(key);
  if(event == HPX_PROF_NO_RESULT){
    return;
  }

  uint64_t seconds, ns, average = 0;
  for(int i = 0; i < _profile_log.events[event].num_entries; i++){
    if(_profile_log.events[event].entries[i].marked){
      average += hpx_time_diff_ns(TIME_NULL,
                                  _profile_log.events[event].entries[i].run_time);
    }
  }
  if (_profile_log.events[event].num_entries == 0) {
    dbg_error("profiler event has no entries for average.\n");
  }
  average /= _profile_log.events[event].num_entries;
  seconds = average / 1e9;
  ns = average % (int64_t)1e9;

  *avg = hpx_time_construct(seconds, ns);
}

void prof_get_total_time(char *key, hpx_time_t *tot){
  int event = _get_event_num(key);
  if(event == HPX_PROF_NO_RESULT){
    return;
  }

  uint64_t seconds, ns, total = 0;
  for(int i = 0; i < _profile_log.events[event].num_entries; i++){
    if(_profile_log.events[event].entries[i].marked){
      total += hpx_time_diff_ns(TIME_NULL,
                                _profile_log.events[event].entries[i].run_time);
    }
  }
  seconds = total / 1e9;
  ns = total % (int64_t)1e9;

  *tot = hpx_time_construct(seconds, ns);
}

void prof_get_min_time(char *key, hpx_time_t *min){
  int event = _get_event_num(key);
  if(event == HPX_PROF_NO_RESULT){
    return;
  }

  int64_t seconds, ns, temp;
  int64_t minimum = 0;
  int start = _profile_log.events[event].num_entries;

  if(_profile_log.events[event].num_entries > 0 ){
    for(int i = 0; i < _profile_log.events[event].num_entries; i++){
      if(_profile_log.events[event].entries[i].marked){
        minimum =
             hpx_time_diff_ns(TIME_NULL,
                              _profile_log.events[event].entries[0].run_time);
        start = i+1;
        break;
      }
    }
  }
  for(int i = start; i < _profile_log.events[event].num_entries; i++){
    if(_profile_log.events[event].entries[i].marked){
      temp = hpx_time_diff_ns(TIME_NULL,
                              _profile_log.events[event].entries[i].run_time);
      if(temp < minimum){
        minimum = temp;
      }
    }
  }
  seconds = minimum / 1e9;
  ns = minimum % (int64_t)1e9;

  *min = hpx_time_construct(seconds, ns);
}

void prof_get_max_time(char *key, hpx_time_t *max){
  int event = _get_event_num(key);
  if(event == HPX_PROF_NO_RESULT){
    return;
  }

  int64_t seconds, ns, temp;
  int64_t maximum = 0;
  int start = _profile_log.events[event].num_entries;

  if(_profile_log.events[event].num_entries > 0 ){
    for(int i = 0; i < _profile_log.events[event].num_entries; i++){
      if(_profile_log.events[event].entries[i].marked){
        maximum =
             hpx_time_diff_ns(TIME_NULL,
                              _profile_log.events[event].entries[0].run_time);
        start = i+1;
        break;
      }
    }
  }
  for(int i = start; i < _profile_log.events[event].num_entries; i++){
    if(_profile_log.events[event].entries[i].marked){
      temp = hpx_time_diff_ns(TIME_NULL,
                              _profile_log.events[event].entries[i].run_time);
      if(temp > maximum){
        maximum = temp;
      }
    }
  }
  seconds = maximum / 1e9;
  ns = maximum % (int64_t)1e9;

  *max = hpx_time_construct(seconds, ns);
}

int prof_get_num_counters(){
  return _profile_log.num_counters;
}

hpx_time_t prof_get_duration(){
  hpx_time_t duration;
  hpx_time_t end;
  end = hpx_time_now();

  hpx_time_diff(_profile_log.start_time, end, &duration);
  return duration;
}

void prof_increment_tally(char *key){
  int event = _get_event_num(key);
  if(event == HPX_PROF_NO_RESULT){
    _create_new_list(key, true);
    event = _profile_log.num_events - 1;
  }

  _profile_log.events[event].tally++;
}

void prof_start_timing(char *key, int *tag){
  hpx_time_t now = hpx_time_now();
  int event = _get_event_num(key);
  if(event == HPX_PROF_NO_RESULT){
    _create_new_list(key, true);
    event = _profile_log.num_events - 1;
  }

  // interrupt current timing
  if(_profile_log.current_event >= 0 &&
     !_profile_log.events[_profile_log.current_event].entries[
                          _profile_log.current_entry].marked &&
     !_profile_log.events[_profile_log.current_event].entries[
                          _profile_log.current_entry].paused){
    hpx_time_t dur;
    hpx_time_diff(_profile_log.events[_profile_log.current_event].entries[
                  _profile_log.current_entry].start_time, now, &dur);
    _profile_log.events[_profile_log.current_event].entries[
                        _profile_log.current_entry].run_time =
             _add_times(_profile_log.events[_profile_log.current_event].entries[
                        _profile_log.current_entry].run_time, dur);
  }

  int index = _create_new_entry(event, _profile_log.events[event].simple);
  _profile_log.events[event].entries[index].last_entry = _profile_log.current_entry;
  _profile_log.events[event].entries[index].last_event = _profile_log.current_event;
  _profile_log.current_entry = index;
  _profile_log.current_event = event;
  _profile_log.events[event].entries[index].start_time = hpx_time_now();
  *tag = index;
}

int prof_stop_timing(char *key, int *tag){
  hpx_time_t end = hpx_time_now();
  int event = _get_event_num(key);
  if(event == HPX_PROF_NO_RESULT){
    return HPX_PROF_NO_RESULT;
  }

  if(*tag == HPX_PROF_NO_TAG){
    for(int i = _profile_log.events[event].num_entries - 1; i >= 0; i--){
      if(!_profile_log.events[event].entries[i].marked){
        *tag = i;
        break;
      }
    }
  }
  if(*tag == HPX_PROF_NO_TAG){
    return event;
  }

  if(!_profile_log.events[event].entries[*tag].paused){
    hpx_time_t dur;
    hpx_time_diff(_profile_log.events[event].entries[*tag].start_time, end, &dur);
    _profile_log.events[event].entries[*tag].run_time =
        _add_times(_profile_log.events[event].entries[*tag].run_time, dur);
  }
  _profile_log.events[event].entries[*tag].marked = true;

  // if another event/entry was being measured prior to switching to the current
  // event/entry, then pick up where we left off
  if(_profile_log.events[event].entries[*tag].last_event >= 0){
    _profile_log.current_entry = _profile_log.events[event].entries[*tag].last_entry;
    _profile_log.current_event = _profile_log.events[event].entries[*tag].last_event;
    if(!_profile_log.events[event].entries[_profile_log.current_entry].paused){
      _profile_log.events[_profile_log.current_event].entries
                         [_profile_log.current_entry].start_time
                          = hpx_time_now();
    }
  }
  return event;
}

int prof_start_hardware_counters(char *key, int *tag){
  prof_start_timing(key, tag);
  return LIBHPX_OK;
}

int prof_stop_hardware_counters(char *key, int *tag){
  prof_stop_timing(key, tag);
  return LIBHPX_OK;
}

int prof_pause(char *key, int *tag){
  hpx_time_t end = hpx_time_now();
  int event = _get_event_num(key);
  if(event == HPX_PROF_NO_RESULT){
    return LIBHPX_OK;
  }

  if(*tag == HPX_PROF_NO_TAG){
    for(int i = _profile_log.events[event].num_entries - 1; i >= 0; i--){
      if(!_profile_log.events[event].entries[i].marked &&
         !_profile_log.events[event].entries[i].paused){
        *tag = i;
        break;
      }
    }
  }
  if(*tag == HPX_PROF_NO_TAG ||
     _profile_log.events[event].entries[*tag].marked ||
     _profile_log.events[event].entries[*tag].paused){
    return LIBHPX_OK;
  }

  // first store timing information
  hpx_time_t dur;
  hpx_time_diff(_profile_log.events[event].entries[*tag].start_time, end, &dur);
  _profile_log.events[event].entries[*tag].run_time =
      _add_times(_profile_log.events[event].entries[*tag].run_time, dur);

  _profile_log.events[event].entries[*tag].paused = true;
  return LIBHPX_OK;
}

int prof_resume(char *key, int *tag){
  int event = _get_event_num(key);
  if(event == HPX_PROF_NO_RESULT){
    return LIBHPX_OK;
  }

  if(*tag == HPX_PROF_NO_TAG){
    for(int i = _profile_log.events[event].num_entries - 1; i >= 0; i--){
      if(!_profile_log.events[event].entries[i].marked &&
         _profile_log.events[event].entries[i].paused){
        *tag = i;
        break;
      }
    }
  }
  if(*tag == HPX_PROF_NO_TAG){
    return LIBHPX_OK;
  }

  _profile_log.events[event].entries[*tag].paused = false;
  _profile_log.events[event].entries[*tag].start_time = hpx_time_now();
  return LIBHPX_OK;
}

