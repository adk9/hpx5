
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)

  Time Operations Function Definitions
  hpx_time.h

  Copyright (c) 2013, Trustees of Indiana University
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/

#ifdef __APPLE__
  #include <mach/mach_time.h>
#endif

#ifdef __linux__
  #include <time.h>
#endif

#pragma once
#ifndef LIBHPX_TIME_H_
#define LIBHPX_TIME_H_

#ifdef __APPLE__
typedef uint64_t hpx_timer_t;
#endif

#ifdef __linux__
typedef struct timespec hpx_timer_t;
#endif

/*
 --------------------------------------------------------------------
  Timer Operations
 --------------------------------------------------------------------
*/

void hpx_timer_init(void);

/* Return the current time per the resolution of the platform-specific
   internal clock. The default resolution used is nanoseconds.
*/
void hpx_get_time(hpx_timer_t *time);

/* Given a start time (ns), returns the elapsed time in microseconds. */
double hpx_elapsed_us(hpx_timer_t start_time);

#endif
