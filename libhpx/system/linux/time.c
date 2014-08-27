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
#ifndef __linux__
#error The HPX time implementation is configured incorrectly
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/// ----------------------------------------------------------------------------
/// @file libhpx/platform/linux/time.c
/// @brief Implements HPX's time interface on linux.
/// ----------------------------------------------------------------------------
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include "hpx/hpx.h"
#include "libhpx/debug.h"


hpx_time_t
hpx_time_now(void) {
  hpx_time_t time;
  if (clock_gettime(CLOCK_MONOTONIC, &time)) {
    dbg_error("system: hpx_time_now() failed to get time.\n");
  }
  return time;
}

static double
_diff_ns(hpx_time_t from, hpx_time_t to) {
  return (double)(((to.tv_sec - from.tv_sec) * 1e9) + (to.tv_nsec - from.tv_nsec));
}

double
hpx_time_diff_us(hpx_time_t from, hpx_time_t to) {
  return _diff_ns(from, to)/1e3;
}

double
hpx_time_diff_ms(hpx_time_t from, hpx_time_t to) {
  return _diff_ns(from, to)/1e6;
}

double
hpx_time_elapsed_us(hpx_time_t from) {
  return hpx_time_diff_us(from, hpx_time_now());
}

double
hpx_time_elapsed_ms(hpx_time_t from) {
  return hpx_time_diff_ms(from, hpx_time_now());
}

static double
_ns(hpx_time_t time) {
  return (time.tv_sec * 1e9) + time.tv_nsec;
}

double
hpx_time_us(hpx_time_t time) {
  return _ns(time)/1e3;
}

double
hpx_time_ms(hpx_time_t time) {
  return _ns(time)/1e6;
}
