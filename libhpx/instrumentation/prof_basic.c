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

static void _prof_begin(){
  _profile_log.start_time = hpx_time_now();
}

static void _prof_end(){
  _profile_log.end_time = hpx_time_now();
}

static void _create_new_list(profile_list_t *list, profile_list_t new_list,
                      char *key, bool simple){
  if(_profile_log.num_entries == _profile_log.max_entries){
    _profile_log.max_entries *= 2;
    profile_list_t *new_list_list = malloc(_profile_log.max_entries *
                                           sizeof(profile_list_t));
    for(int i = 0; i < _profile_log.num_entries; i++){
      new_list_list[i] = list[i];
    }
    free(list);
    list = new_list_list;
    _profile_log.entries = list;
  }
  int index = _profile_log.num_entries;
  _profile_log.num_entries++;
  list[index] = new_list;
  list[index].entries = malloc(_profile_log.max_entries *
                               sizeof(struct profile_entry));
  list[index].tally = 0;
  list[index].num_entries = 0;
  list[index].max_entries = _profile_log.max_entries;
  list[index].key = key;
  list[index].simple = simple;
}

static void _create_new_entry(struct profile_entry *entries,
                       struct profile_entry new_entry,
                       int event, bool simple){
  if(_profile_log.entries[event].num_entries ==
     _profile_log.entries[event].max_entries){
    _profile_log.entries[event].max_entries *= 2;
    struct profile_entry *new_list =
                                malloc(_profile_log.entries[event].max_entries *
                                       sizeof(struct profile_entry));
    for(int i = 0; i < _profile_log.entries[event].num_entries; i++){
      new_list[i] = entries[i];
    }
    free(entries);
    entries = new_list;
    _profile_log.entries[event].entries = entries;
  }
  int index = _profile_log.entries[event].num_entries;
  _profile_log.entries[event].tally++;
  _profile_log.entries[event].num_entries++;
  entries[index] = new_entry;
  entries[index].counter_totals = NULL;
  entries[index].marked = false;
}

//returns index of matching key/creates new entry if index doesn't exist
static int _get_event_num(char *key){
  for(int i = 0; i < _profile_log.num_entries; i++){
    if(strcmp(key, _profile_log.entries[i].key) == 0){
      return i;
    }
  }
  return -1;
}

void prof_init(struct config *cfg){
  _profile_log.counters = NULL;
  _profile_log.counter_names = NULL;
  _profile_log.num_counters = 0;
  _profile_log.entries = malloc(_profile_log.max_entries *
                                sizeof(profile_list_t));

  if(cfg->prof_action){
    profile_list_t new_list;
    _create_new_list(_profile_log.entries, new_list, cfg->prof_action, false);
  }
  _prof_begin();
}

int prof_fini(){
  if(_profile_log.num_counters > 0){
    _prof_end();
    inst_prof_dump(_profile_log);
    for(int i = 0; i < _profile_log.num_entries; i++){
      free(_profile_log.entries[i].entries);
    }
    free(_profile_log.counters);
    free(_profile_log.counter_names);
  }
  return LIBHPX_OK;
}

int prof_get_averages(int64_t *values, int num_values, char *key){
  return LIBHPX_OK;
}

int prof_get_totals(int64_t *values, int num_values, char *key){
  return LIBHPX_OK;
}

size_t prof_get_tally(char *key){
  int event = _get_event_num(key);
  if(event == -1){
    return 0;
  }
  return _profile_log.entries[event].tally;
}

void prof_get_average_time(char *key, hpx_time_t *avg){
  int event = _get_event_num(key);
  if(event == -1){
    return;
  }

  uint64_t seconds, ns, average = 0;
  for(int i = 0; i < _profile_log.entries[event].num_entries; i++){
    if(_profile_log.entries[event].entries[i].marked){
      average += hpx_time_diff_ns(_profile_log.entries[event].entries[i].start_time,
                                _profile_log.entries[event].entries[i].end_time);
    }
  }
  if (_profile_log.entries[event].num_entries == 0) {
    dbg_error("profiler event has no entries for average.\n");
  }
  average /= _profile_log.entries[event].num_entries;
  seconds = average / 1e9;
  ns = average % (long)1e9;

  *avg = hpx_time_construct(seconds, ns);
}

void prof_get_total_time(char *key, hpx_time_t *tot){
  int event = _get_event_num(key);
  if(event == -1){
    return;
  }

  uint64_t seconds, ns, total = 0;
  for(int i = 0; i < _profile_log.entries[event].num_entries; i++){
    if(_profile_log.entries[event].entries[i].marked){
      total += hpx_time_diff_ns(_profile_log.entries[event].entries[i].start_time,
                                _profile_log.entries[event].entries[i].end_time);
    }
  }
  seconds = total / 1e9;
  ns = total % (long)1e9;

  *tot = hpx_time_construct(seconds, ns);
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
  if(event == -1){
    profile_list_t new_list;
    _create_new_list(_profile_log.entries, new_list, key, true);
    event = _profile_log.num_entries-1;
  }

  _profile_log.entries[event].tally++;
}

void prof_start_timing(char *key){
  int event = _get_event_num(key);
  if(event == -1){
    profile_list_t new_list;
    _create_new_list(_profile_log.entries, new_list, key, true);
    event = _profile_log.num_entries-1;
  }

  struct profile_entry entry;
  _create_new_entry(_profile_log.entries[event].entries, entry, event,
                    _profile_log.entries[event].simple);
  _profile_log.entries[event].entries[
                       _profile_log.entries[
                       event].num_entries - 1].start_time = hpx_time_now();
}

void prof_stop_timing(char *key){
  int event = _get_event_num(key);
  if(event == -1){
    return;
  }
  for(int i = _profile_log.entries[event].num_entries-1; i >= 0; i--){
    if(!_profile_log.entries[event].entries[i].marked){
      _profile_log.entries[event].entries[i].end_time = hpx_time_now();
      _profile_log.entries[event].entries[i].marked = true;
      break;
    }
  }
}

int prof_start_hardware_counters(char *key){
  prof_start_timing(key);
  return LIBHPX_OK;
}

int prof_stop_hardware_counters(char *key){
  prof_stop_timing(key);
  return LIBHPX_OK;
}

