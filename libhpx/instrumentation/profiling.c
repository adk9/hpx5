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

#ifdef HAVE_PAPI
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <hpx/hpx.h>
#include <libsync/sync.h>
#include <libhpx/config.h>
#include <libhpx/debug.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/profiling.h>

/// Each locality maintains a single profile log
static profile_t _profile_log = PROFILE_INIT;

void set_event(size_t papi_event, size_t bit, size_t bitset, 
               int max_counters, int *num_counters){
  if(!(bit | bitset) || (PAPI_query_event(papi_event) != PAPI_OK)
                     || (*num_counters >= max_counters)){
    return;
  }

  _profile_log.counter_totals[*num_counters] = 0;
  _profile_log.counters[*num_counters] = papi_event;
  *num_counters+=1;
}

void print_warning(size_t papi_event){
  fprintf(stderr, "Warning: ");
  switch(papi_event){
    case PAPI_L1_TCM:  fprintf(stderr, "PAPI_L1_TCM");
                       break;
    case PAPI_L2_TCM:  fprintf(stderr, "PAPI_L2_TCM");
                       break;
    case PAPI_L3_TCM:  fprintf(stderr, "PAPI_L3_TCM");
                       break;
    case PAPI_TLB_TL:  fprintf(stderr, "PAPI_TLB_TL");
                       break;
    case PAPI_TOT_INS: fprintf(stderr, "PAPI_TOT_INS");
                       break;
    case PAPI_INT_INS: fprintf(stderr, "PAPI_INT_INS");
                       break;
    case PAPI_FP_INS:  fprintf(stderr, "PAPI_FP_INS");
                       break;
    case PAPI_LD_INS:  fprintf(stderr, "PAPI_LD_INS");
                       break;
    case PAPI_SR_INS:  fprintf(stderr, "PAPI_SR_INS");
                       break;
    case PAPI_BR_INS:  fprintf(stderr, "PAPI_BR_INS");
                       break;
    case PAPI_TOT_CYC: fprintf(stderr, "PAPI_TOT_CYC");
                       break;
    default:           fprintf(stderr, "PAPI counter");
                       break;
  }
}

void test_event(size_t papi_event, size_t bit, size_t bitset, int max_counters,
                int *num_counters){
  if(!(bit & bitset)){
    return;
  }

  if(PAPI_query_event(papi_event) != PAPI_OK){
    print_warning(papi_event);
    fprintf(stderr, " is not available on this system\n");
    return;
  }

  *num_counters+=1;
  if(*num_counters > max_counters){
    print_warning(papi_event);
    printf(" could not be included in profiling due to limited resources\n");
  }
}

int prof_init(struct config *cfg){
#ifndef ENABLE_INSTRUMENTATION
  return LIBHPX_OK;
#endif

  int max_counters = PAPI_num_counters();
  int num_counters = 0;
  size_t counters = cfg->prof_counters;

  if(counters == 0){
    return LIBHPX_OK;
  }
  
  int eventset = PAPI_NULL;
  int retval = PAPI_library_init(PAPI_VER_CURRENT);
  if(retval != PAPI_VER_CURRENT){
    fprintf(stderr, "unable to initialize PAPI\n");
    return retval;
  }
  retval = PAPI_create_eventset(&eventset);
  if(retval != PAPI_OK){
    fprintf(stderr, "unable to create eventset\n");
    return retval;
  }

  //test events first to generate warnings and determine which counters
  //can have space allocated for them
  test_event(PAPI_L1_TCM, HPX_L1_TCM, counters, max_counters, &num_counters);
  test_event(PAPI_L2_TCM, HPX_L2_TCM, counters, max_counters, &num_counters);
  test_event(PAPI_L3_TCM, HPX_L3_TCM, counters, max_counters, &num_counters);
  test_event(PAPI_TLB_TL, HPX_TLB_TL, counters, max_counters, &num_counters);
  test_event(PAPI_TOT_INS, HPX_TOT_INS, counters, max_counters, &num_counters);
  test_event(PAPI_INT_INS, HPX_INT_INS, counters, max_counters, &num_counters);
  test_event(PAPI_FP_INS, HPX_FP_INS, counters, max_counters, &num_counters);
  test_event(PAPI_LD_INS, HPX_LD_INS, counters, max_counters, &num_counters);
  test_event(PAPI_SR_INS, HPX_SR_INS, counters, max_counters, &num_counters);
  test_event(PAPI_BR_INS, HPX_BR_INS, counters, max_counters, &num_counters);
  test_event(PAPI_TOT_CYC, HPX_TOT_CYC, counters, max_counters, &num_counters);

  if(num_counters > max_counters){
    fprintf(stderr, "Note: maximum available counters is %d\n", max_counters);
    num_counters = max_counters;
  }

  _profile_log.counters = malloc(num_counters * sizeof(int));
  _profile_log.counter_totals = malloc(num_counters * sizeof(long long));

  max_counters = num_counters;
  num_counters = 0;
  //actually set the events
  set_event(PAPI_L1_TCM, HPX_L1_TCM, counters, max_counters, &num_counters);  
  set_event(PAPI_L2_TCM, HPX_L2_TCM, counters, max_counters, &num_counters);  
  set_event(PAPI_L3_TCM, HPX_L3_TCM, counters, max_counters, &num_counters);  
  set_event(PAPI_TLB_TL, HPX_TLB_TL, counters, max_counters, &num_counters);  
  set_event(PAPI_TOT_INS, HPX_TOT_INS, counters, max_counters, &num_counters);
  set_event(PAPI_INT_INS, HPX_INT_INS, counters, max_counters, &num_counters);
  set_event(PAPI_FP_INS, HPX_FP_INS, counters, max_counters, &num_counters);  
  set_event(PAPI_LD_INS, HPX_LD_INS, counters, max_counters, &num_counters);  
  set_event(PAPI_SR_INS, HPX_SR_INS, counters, max_counters, &num_counters);  
  set_event(PAPI_BR_INS, HPX_BR_INS, counters, max_counters, &num_counters);  
  set_event(PAPI_TOT_CYC, HPX_TOT_CYC, counters, max_counters, &num_counters);

  return LIBHPX_OK;
}

void prof_begin(){
  _profile_log.start_time = hpx_time_now();
  _profile_log.tally = 0;
  for(int i = 0; i < _profile_log.num_counters; i++){
    _profile_log.counter_totals[i] = 0;
  }
}

int prof_end(){
  _profile_log.end_time = hpx_time_now();
  int retval = PAPI_OK;
  if(_profile_log.papi_running){
    retval = PAPI_stop_counters(NULL, 0);
    _profile_log.papi_running = false;
  }
  return retval;
}

void prof_tally_mark(){
  if(_profile_log.papi_running){
    _profile_log.tally++;
  }
}

int prof_get_averages(long long *values, int num_values){
  if(num_values != _profile_log.num_counters){
    for(int i = 0; i < num_values; i++){
      values[i] = -1;
    }
    return -1;
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
    for(int i = 0; i < num_values; i++){
      values[i] = -1;
    }
    return -1;
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
  if(_profile_log.papi_running){
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

  if(_profile_log.papi_running){
    _profile_log.papi_running = false;
    retval = PAPI_stop_counters(NULL, 0);
  }
  return retval;
}

// TODO: everything here
int prof_fini(){
  return 0;
}

int prof_start_papi_counters(){
  int retval = PAPI_start_counters(_profile_log.counters, _profile_log.num_counters);
  if(retval == PAPI_OK){
    _profile_log.papi_running = true;
  }
  return retval;
}

int prof_stop_papi_counters(long long *values, int num_values){
  if(num_values != _profile_log.num_counters){
    for(int i = 0; i < num_values; i++){
      values[i] = -1;
    }
    return -1;
  }
  int retval = PAPI_stop_counters(values, num_values);
  if(retval != PAPI_OK){
    return retval;
  }
  
  for(int i = 0; i < num_values; i++){
    _profile_log.counter_totals[i] += values[i];
  }

  _profile_log.papi_running = false;
  return PAPI_OK;
}

int prof_read_papi_counters(long long *values, int num_values){
  if(num_values != _profile_log.num_counters){
    for(int i = 0; i < num_values; i++){
      values[i] = -1;
    }
    return -1;
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
    for(int i = 0; i < num_values; i++){
      values[i] = -1;
    }
    return -1;
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

#endif
