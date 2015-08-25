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
#include <papi.h>
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

void _prof_begin(){
  _profile_log.start_time = hpx_time_now();
}

void _prof_end(){
  _profile_log.end_time = hpx_time_now();
}

static const char* _get_counter_string(size_t papi_event){
  switch(papi_event){
    case PAPI_L1_TCM:  return "PAPI_L1_TCM";
    case PAPI_L2_TCM:  return "PAPI_L2_TCM";
    case PAPI_L3_TCM:  return "PAPI_L3_TCM";
    case PAPI_TLB_TL:  return "PAPI_TLB_TL";
    case PAPI_TOT_INS: return "PAPI_TOT_INS";
    case PAPI_INT_INS: return "PAPI_INT_INS";
    case PAPI_FP_INS:  return "PAPI_FP_INS";
    case PAPI_LD_INS:  return "PAPI_LD_INS";
    case PAPI_SR_INS:  return "PAPI_SR_INS";
    case PAPI_BR_INS:  return "PAPI_BR_INS";
    case PAPI_TOT_CYC: return "PAPI_TOT_CYC";
    default:           return "PAPI counter";
  }
}

static void _set_event(size_t papi_event, size_t bit, size_t bitset, 
                       int max_counters, int *num_counters){
  if(!(bit & bitset) || (PAPI_query_event(papi_event) != PAPI_OK)
                     || (*num_counters >= max_counters)){
    return;
  }

  for(int i = 0; i < _profile_log.max_entries; i++){
    _profile_log.counters[*num_counters] = papi_event;
    _profile_log.counter_names[*num_counters] = _get_counter_string(papi_event);
    *num_counters+=1;
  }
}

static void _test_event(size_t papi_event, size_t bit, size_t bitset,
                        int max_counters, int *num_counters){
  if(!(bit & bitset)){
    return;
  }

  if(PAPI_query_event(papi_event) != PAPI_OK){
    const char *counter_name = _get_counter_string(papi_event);
    
    log_error("Warning: %s is not available on this system\n", counter_name);
    return;
  }

  *num_counters+=1;
  if(*num_counters > max_counters){
    const char *counter_name = _get_counter_string(papi_event);
    log_error("Warning: %s could not be included in profiling due to limited "
              "resources\n", counter_name);
  }
}

void _create_new_list(profile_list_t *list, profile_list_t new_list, 
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

void _create_new_entry(struct profile_entry *entries, 
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
  if(simple){
    entries[index].counter_totals = NULL;
  }
  else{
    entries[index].counter_totals = malloc(_profile_log.num_counters * 
                                           sizeof(long long));
  }
  entries[index].marked = false;
}

//returns index of matching key/creates new entry if index doesn't exist
int _get_event_num(char *key){
  for(int i = 0; i < _profile_log.num_entries; i++){
    if(strcmp(key, _profile_log.entries[i].key) == 0){
      return i;
    }
  }
  return -1;
}

void prof_init(struct config *cfg){
  int max_counters = PAPI_num_counters();
  int num_counters = 0;
  size_t counters = cfg->prof_counters;

  int eventset = PAPI_NULL;
  int retval = PAPI_library_init(PAPI_VER_CURRENT);
  if(retval != PAPI_VER_CURRENT){
    log_error("unable to initialize PAPI with error code %d\n", retval);
  }
  retval = PAPI_create_eventset(&eventset);
  if(retval != PAPI_OK){
    log_error("unable to create eventset with error code %d\n", retval);
  }

  //test events first to generate warnings and determine which counters
  //can have space allocated for them
  _test_event(PAPI_L1_TCM, HPX_PAPI_L1_TCM, counters, 
              max_counters, &num_counters);
  _test_event(PAPI_L2_TCM, HPX_PAPI_L2_TCM, counters, 
              max_counters, &num_counters);
  _test_event(PAPI_L3_TCM, HPX_PAPI_L3_TCM, counters, 
              max_counters, &num_counters);
  _test_event(PAPI_TLB_TL, HPX_PAPI_TLB_TL, counters, 
              max_counters, &num_counters);
  _test_event(PAPI_TOT_INS, HPX_PAPI_TOT_INS, counters,
              max_counters, &num_counters);
  _test_event(PAPI_INT_INS, HPX_PAPI_INT_INS, counters, 
              max_counters, &num_counters);
  _test_event(PAPI_FP_INS, HPX_PAPI_FP_INS, counters, 
              max_counters, &num_counters);
  _test_event(PAPI_LD_INS, HPX_PAPI_LD_INS, counters, 
              max_counters, &num_counters);
  _test_event(PAPI_SR_INS, HPX_PAPI_SR_INS, counters, 
              max_counters, &num_counters);
  _test_event(PAPI_BR_INS, HPX_PAPI_BR_INS, counters, 
              max_counters, &num_counters);
  _test_event(PAPI_TOT_CYC, HPX_PAPI_TOT_CYC, counters, 
              max_counters, &num_counters);

  if(num_counters > max_counters){
    log_error("Note: maximum available counters is %d\n", max_counters);
    num_counters = max_counters;
  }

  _profile_log.counters = malloc(num_counters * sizeof(int));
  _profile_log.counter_names = malloc(num_counters * sizeof(char*));
  _profile_log.num_counters = num_counters;
  _profile_log.entries = malloc(_profile_log.max_entries * 
                                sizeof(profile_list_t));

  max_counters = num_counters;
  num_counters = 0;

  //actually set the events
  _set_event(PAPI_L1_TCM, HPX_PAPI_L1_TCM, counters, 
             max_counters, &num_counters);  
  _set_event(PAPI_L2_TCM, HPX_PAPI_L2_TCM, counters,
             max_counters, &num_counters);  
  _set_event(PAPI_L3_TCM, HPX_PAPI_L3_TCM, counters,
             max_counters, &num_counters);  
  _set_event(PAPI_TLB_TL, HPX_PAPI_TLB_TL, counters,
             max_counters, &num_counters);  
  _set_event(PAPI_TOT_INS, HPX_PAPI_TOT_INS, counters,
             max_counters, &num_counters);
  _set_event(PAPI_INT_INS, HPX_PAPI_INT_INS, counters,
             max_counters, &num_counters);
  _set_event(PAPI_FP_INS, HPX_PAPI_FP_INS, counters,
             max_counters, &num_counters);  
  _set_event(PAPI_LD_INS, HPX_PAPI_LD_INS, counters,
             max_counters, &num_counters);  
  _set_event(PAPI_SR_INS, HPX_PAPI_SR_INS, counters,
             max_counters, &num_counters);  
  _set_event(PAPI_BR_INS, HPX_PAPI_BR_INS, counters,
             max_counters, &num_counters);  
  _set_event(PAPI_TOT_CYC, HPX_PAPI_TOT_CYC, counters,
             max_counters, &num_counters);

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
      if(!_profile_log.entries[i].simple){
        for(int j = 0; j < _profile_log.entries[i].num_entries; j++){
          free(_profile_log.entries[i].entries[j].counter_totals);
        }
      }
      free(_profile_log.entries[i].entries);
    }
    free(_profile_log.counters);
    free(_profile_log.counter_names);
  }
  return LIBHPX_OK;
}

int prof_get_averages(long long *values, int num_values, char *key){
  if(num_values != _profile_log.num_counters){
    return PAPI_EINVAL;
  }
  int event = _get_event_num(key);
  if(event == -1 ||_profile_log.entries[event].simple){
    return PAPI_EINVAL;
  }

  double divisor = _profile_log.entries[event].num_entries;

  if(divisor == 0){
    return PAPI_ENOTRUN;
  }

  for(int i = 0; i < num_values; i++){
    values[i] = 0;
    for(int j = 0; j < _profile_log.entries[event].num_entries; j++){
      values[i] += _profile_log.entries[event].entries[j].counter_totals[i];
    }
    values[i] /= divisor;
  }
  return PAPI_OK;
}

int prof_get_totals(long long *values, int num_values, char *key){
  if(num_values != _profile_log.num_counters){
    return PAPI_EINVAL;
  }
  int event = _get_event_num(key);
  if(event == -1 || _profile_log.entries[event].simple){
    return PAPI_EINVAL;
  }

  for(int i = 0; i < num_values; i++){
    values[i] = 0;
    for(int j = 0; j < _profile_log.entries[event].num_entries; j++){
      values[i] += _profile_log.entries[event].entries[j].counter_totals[i];
    }
  }
  return PAPI_OK;
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

  unsigned long seconds, ns, average = 0;
  for(int i = 0; i < _profile_log.entries[event].num_entries; i++){
    average += 
             hpx_time_diff_ns(_profile_log.entries[event].entries[i].start_time,
                              _profile_log.entries[event].entries[i].end_time);
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

  int64_t seconds, ns, total = 0;
  for(int i = 0; i < _profile_log.entries[event].num_entries; i++){
    total += hpx_time_diff_ns(_profile_log.entries[event].entries[i].start_time,
                              _profile_log.entries[event].entries[i].end_time);
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
  int event = _get_event_num(key);
  if(_profile_log.entries[event].simple){
    return PAPI_EINVAL;
  }
  
  int retval = PAPI_start_counters(_profile_log.counters, 
                                   _profile_log.num_counters);
  return retval;
}

int prof_stop_hardware_counters(char *key){
  prof_stop_timing(key);
  long long values[_profile_log.num_counters];
  int event = _get_event_num(key);
  if(event == -1 || _profile_log.entries[event].simple){
    return PAPI_EINVAL;
  }
  int retval = PAPI_read_counters(values, _profile_log.num_counters);
  if(retval != PAPI_OK){
    return retval;
  }
  
  int where = -1;
  for(int i = _profile_log.entries[event].num_entries-1; i >= 0; i--){
    if(!_profile_log.entries[event].entries[i].marked){
      where = i;
      _profile_log.entries[event].entries[i].marked = true;
      break;
    }
  }
  if(where == -1){
    return -1;
  }
  for(int i = 0; i < _profile_log.num_counters; i++){
    _profile_log.entries[event].entries[where].counter_totals[i] = values[i];
  }

  return PAPI_OK;
}

