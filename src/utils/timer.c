/*
 ====================================================================
  High Performance ParalleX Library (libhpx)

  Copyright (c) 2013, Trustees of Indiana University
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/

#include <assert.h>

#include "hpx/utils/timer.h"

#ifdef __APPLE__
static mach_timebase_info_data_t tbi;
#endif

/*
 --------------------------------------------------------------------
  hpx_time_init

  Initialize the time subsystem. This function should be called
  before using any other timer functions.
  -------------------------------------------------------------------
*/
void hpx_timer_init(void) {
#ifdef __APPLE__
  if (tbi.denom == 0) {
    (void) mach_timebase_info(&tbi);
  }
#endif
}

/*
 --------------------------------------------------------------------
  hpx_get_time

  Returns the current time in nanoseconds.
  -------------------------------------------------------------------
*/
void hpx_get_time(hpx_timer_t *time) {
#ifdef __APPLE__
  assert(time);
  *time = mach_absolute_time();
#endif
#ifdef __linux__
  clock_gettime(CLOCK_MONOTONIC, time);
#endif
}

/*
 --------------------------------------------------------------------
  hpx_elapsed_us

  Given a timer handle, return the elapsed time (between current time
  and time indicated by the timer handle "start_time") in
  microseconds.
  -------------------------------------------------------------------
*/
double hpx_elapsed_us(hpx_timer_t start_time) {
  hpx_timer_t end_time;
  hpx_get_time(&end_time);
#ifdef __APPLE__
  assert(tbi.denom!=0);
  return (((end_time - start_time) * tbi.numer / tbi.denom) / 1e6);
#endif
#ifdef __linux__
  unsigned long elapsed = ((end_time.tv_sec * 1e9) + end_time.tv_nsec) - ((start_time.tv_sec * 1e9) + start_time.tv_nsec);
  return (elapsed / 1e6);
#endif
}
