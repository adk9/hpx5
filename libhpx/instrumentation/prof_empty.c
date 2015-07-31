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
#include <hpx/hpx.h>
#include <libhpx/config.h>

void prof_init(struct config *cfg){
}

void prof_begin(){
}

int prof_end(long long *values, int num_values){
  return 0;
}

void prof_tally_mark(){
}

int prof_get_averages(long long *values, int num_values){
  return 0;
}

int prof_get_totals(long long *values, int num_values){
  return 0;
}

size_t prof_get_tally(){
  return 0;
}

int prof_get_num_counters(){
  return 0;
}

hpx_time_t prof_get_duration(){
  hpx_time_t duration = hpx_time_now();

  hpx_time_diff(duration, duration, &duration);
  return duration;
}

int prof_reset(){
  return 0;
}

int prof_fini(){
  return 0;
}

int prof_start_papi_counters(){
  return 0;
}

int prof_stop_papi_counters(long long *values, int num_values){
  return 0;
}

int prof_read_papi_counters(long long *values, int num_values){
  return 0;
}

int prof_accum_papi_counters(long long *values, int num_values){
  return 0;
}

