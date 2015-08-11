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

  _profile_log.counter_totals[*num_counters] = 0;
  _profile_log.counters[*num_counters] = papi_event;
  _profile_log.counter_names[*num_counters] = _get_counter_string(papi_event);
  *num_counters+=1;
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

void prof_init(struct config *cfg){
  int max_counters = PAPI_num_counters();
  int num_counters = 0;
  size_t counters = cfg->prof_counters;

  if(counters == 0){
    return;
  }
  
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
  _test_event(PAPI_L1_TCM, HPX_PAPI_L1_TCM, counters, max_counters, &num_counters);
  _test_event(PAPI_L2_TCM, HPX_PAPI_L2_TCM, counters, max_counters, &num_counters);
  _test_event(PAPI_L3_TCM, HPX_PAPI_L3_TCM, counters, max_counters, &num_counters);
  _test_event(PAPI_TLB_TL, HPX_PAPI_TLB_TL, counters, max_counters, &num_counters);
  _test_event(PAPI_TOT_INS, HPX_PAPI_TOT_INS, counters, max_counters, &num_counters);
  _test_event(PAPI_INT_INS, HPX_PAPI_INT_INS, counters, max_counters, &num_counters);
  _test_event(PAPI_FP_INS, HPX_PAPI_FP_INS, counters, max_counters, &num_counters);
  _test_event(PAPI_LD_INS, HPX_PAPI_LD_INS, counters, max_counters, &num_counters);
  _test_event(PAPI_SR_INS, HPX_PAPI_SR_INS, counters, max_counters, &num_counters);
  _test_event(PAPI_BR_INS, HPX_PAPI_BR_INS, counters, max_counters, &num_counters);
  _test_event(PAPI_TOT_CYC, HPX_PAPI_TOT_CYC, counters, max_counters, &num_counters);

  if(num_counters > max_counters){
    log_error("Note: maximum available counters is %d\n", max_counters);
    num_counters = max_counters;
  }

  _profile_log.counters = malloc(num_counters * sizeof(int));
  _profile_log.counter_totals = malloc(num_counters * sizeof(long long));
  _profile_log.counter_names = malloc(num_counters * sizeof(char*));
  _profile_log.num_counters = num_counters;

  max_counters = num_counters;
  num_counters = 0;
  //actually set the events
  _set_event(PAPI_L1_TCM, HPX_PAPI_L1_TCM, counters, max_counters, &num_counters);  
  _set_event(PAPI_L2_TCM, HPX_PAPI_L2_TCM, counters, max_counters, &num_counters);  
  _set_event(PAPI_L3_TCM, HPX_PAPI_L3_TCM, counters, max_counters, &num_counters);  
  _set_event(PAPI_TLB_TL, HPX_PAPI_TLB_TL, counters, max_counters, &num_counters);  
  _set_event(PAPI_TOT_INS, HPX_PAPI_TOT_INS, counters, max_counters, &num_counters);
  _set_event(PAPI_INT_INS, HPX_PAPI_INT_INS, counters, max_counters, &num_counters);
  _set_event(PAPI_FP_INS, HPX_PAPI_FP_INS, counters, max_counters, &num_counters);  
  _set_event(PAPI_LD_INS, HPX_PAPI_LD_INS, counters, max_counters, &num_counters);  
  _set_event(PAPI_SR_INS, HPX_PAPI_SR_INS, counters, max_counters, &num_counters);  
  _set_event(PAPI_BR_INS, HPX_PAPI_BR_INS, counters, max_counters, &num_counters);  
  _set_event(PAPI_TOT_CYC, HPX_PAPI_TOT_CYC, counters, max_counters, &num_counters);
}

void prof_begin(){
  _profile_log.start_time = hpx_time_now();
  _profile_log.tally = 0;
  for(int i = 0; i < _profile_log.num_counters; i++){
    _profile_log.counter_totals[i] = 0;
  }
}

int prof_end(long long *values, int num_values){
  int retval = PAPI_OK;
  if(_profile_log.prof_running && num_values != _profile_log.num_counters){
    return PAPI_EINVAL;
  }
  if(_profile_log.prof_running){
    retval = PAPI_stop_counters(values, num_values);
    _profile_log.prof_running = false;
    for(int i = 0; i < num_values; i++){
      _profile_log.counter_totals[i] += values[i];
    }
  }
  _profile_log.end_time = hpx_time_now();
  return retval;
}

void prof_tally_mark(){
  _profile_log.tally++;
}

int prof_get_averages(long long *values, int num_values){
  if(num_values != _profile_log.num_counters){
    return PAPI_EINVAL;
  }
  double divisor = _profile_log.tally;

  //assume that a tally of 0 means that we are recording values for one event
  if(divisor == 0){
    divisor = 1;
  }

  for(int i = 0; i < num_values; i++){
    values[i] = _profile_log.counter_totals[i]/divisor;
  }
  return PAPI_OK;
}

int prof_get_totals(long long *values, int num_values){
  if(num_values != _profile_log.num_counters){
    return PAPI_EINVAL;
  }

  for(int i = 0; i < num_values; i++){
    values[i] = _profile_log.counter_totals[i];
  }
  return PAPI_OK;
}

size_t prof_get_tally(){
  return _profile_log.tally;
}

int prof_get_num_counters(){
  return _profile_log.num_counters;
}

hpx_time_t prof_get_duration(){
  hpx_time_t duration;
  hpx_time_t end;
  if(_profile_log.prof_running){
    end = hpx_time_now();
  }
  else{
    end = _profile_log.end_time;
  }

  hpx_time_diff(_profile_log.start_time, end, &duration);
  return duration;
}

int prof_reset(){
  int retval = PAPI_OK;
  for(int i = 0; i < _profile_log.num_counters; i++){
    _profile_log.counter_totals[i] = 0;
  }
  _profile_log.start_time = hpx_time_now();
  _profile_log.end_time = _profile_log.start_time;
  _profile_log.tally = 0;

  //I don't like doing this but the alternative is to set PAPI up from the 
  //beginning in a different way, using event sets instead of groups of
  //individual events.  Only event sets have a PAPI_reset() function for some
  //reason, and all PAPI_stop_counter() functions require a valid array.
  if(_profile_log.prof_running){
    _profile_log.prof_running = false;
    long long *dummy = malloc(sizeof(long long) * _profile_log.num_counters);
    retval = PAPI_stop_counters(dummy, _profile_log.num_counters);
    free(dummy);
  }
  return retval;
}

int prof_fini(){
  if(_profile_log.num_counters > 0){
    inst_prof_dump(_profile_log);
    free(_profile_log.counter_totals);
    free(_profile_log.counters);
    free(_profile_log.counter_names);
  }
  return LIBHPX_OK;
}

int prof_start_papi_counters(){
  int retval = PAPI_start_counters(_profile_log.counters, _profile_log.num_counters);
  if(retval == PAPI_OK){
    _profile_log.prof_running = true;
  }
  return retval;
}

int prof_stop_papi_counters(long long *values, int num_values){
  if(num_values != _profile_log.num_counters){
    return PAPI_EINVAL;
  }
  if(!_profile_log.prof_running){
    for(int i = 0; i < num_values; i++){
      values[i] = 0;
    }
    return PAPI_ENOTRUN;
  }

  int retval = PAPI_stop_counters(values, num_values);
  if(retval != PAPI_OK){
    return retval;
  }
  
  for(int i = 0; i < num_values; i++){
    _profile_log.counter_totals[i] += values[i];
  }

  _profile_log.prof_running = false;
  return PAPI_OK;
}

int prof_read_papi_counters(long long *values, int num_values){
  if(num_values != _profile_log.num_counters){
    return PAPI_EINVAL;
  }
  if(!_profile_log.prof_running){
    for(int i = 0; i < num_values; i++){
      values[i] = 0;
    }
    return PAPI_ENOTRUN;
  }
  int retval = PAPI_read_counters(values, num_values);
  if(retval != PAPI_OK){
    return retval;
  }
  
  for(int i = 0; i < num_values; i++){
    _profile_log.counter_totals[i] += values[i];
  }

  return PAPI_OK;
}

int prof_accum_papi_counters(long long *values, int num_values){
  if(num_values != _profile_log.num_counters){
    return PAPI_EINVAL;
  }
  if(!_profile_log.prof_running){
    for(int i = 0; i < num_values; i++){
      values[i] = 0;
    }
    return PAPI_ENOTRUN;
  }
  long long *temp = malloc(sizeof(long long)*num_values);
  int retval = PAPI_read_counters(temp, num_values);
  if(retval != PAPI_OK){
    return retval;
  }
  
  for(int i = 0; i < num_values; i++){
    values[i] += temp[i];
    _profile_log.counter_totals[i] += temp[i];
  }

  free(temp);
  return PAPI_OK;
}

