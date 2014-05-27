// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#ifndef HPX_TIME_H
#define HPX_TIME_H


/// ----------------------------------------------------------------------------
/// HPX high-resolution timer interface
/// ----------------------------------------------------------------------------
#if defined(__linux__)
#include <time.h>
typedef struct timespec hpx_time_t;
#define HPX_TIME_INIT {0}
#elif defined(__APPLE__)
#include <stdint.h>
typedef uint64_t hpx_time_t;
#define HPX_TIME_INIT (0)
#endif


hpx_time_t hpx_time_now(void);
double hpx_time_us(hpx_time_t from);
double hpx_time_ms(hpx_time_t from);
double hpx_time_diff_us(hpx_time_t from, hpx_time_t to);
double hpx_time_diff_ms(hpx_time_t from, hpx_time_t to);
double hpx_time_elapsed_us(hpx_time_t from);
double hpx_time_elapsed_ms(hpx_time_t from);


#endif
