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


/// @file
/// @brief HPX high-resolution timer interface

#if defined(__linux__)
#include <time.h>
typedef struct timespec hpx_time_t;
#define HPX_TIME_INIT {0}
#elif defined(__APPLE__)
#include <stdint.h>
typedef uint64_t hpx_time_t;
#define HPX_TIME_INIT (0)
#endif

/// @struct {hpx_time_t} the type used internally by HPX to represent time

/// Get the current time
/// @returns the current time at the highest resolution possible
hpx_time_t hpx_time_now(void);

/// Get a double representing a time in microseconds
/// @param from the time to convert
/// @returns    the time as converted to a double, in microseconds
double hpx_time_us(hpx_time_t from);

/// Get a double representing a time in milliseconds
/// @param from the time to convert
/// @returns    the time as converted to a double, in milliseconds
double hpx_time_ms(hpx_time_t from);

/// Get a double representing a time span in microseconds
/// @param from the beginning of the time span
/// @param   to the end of the time span to convert
/// @returns the time span as converted to a double, in microseconds
double hpx_time_diff_us(hpx_time_t from, hpx_time_t to);

/// Get a double representing a time span in milliseconds
/// @param from the beginning of the time span
/// @param   to the end of the time span to convert
/// @returns the time span as converted to a double, in milliseconds
double hpx_time_diff_ms(hpx_time_t from, hpx_time_t to);

/// Get the time elapsed since @p from, in microseconds
/// @param from the start time to measure from
/// @returns    the difference between @from and now,
///             in microseconds
double hpx_time_elapsed_us(hpx_time_t from);

/// Get the time elapsed since @p from, in milliseconds
/// @param from the start time to measure from
/// @returns    the difference between @from and now,
///             in milliseconds
double hpx_time_elapsed_ms(hpx_time_t from);


#endif
