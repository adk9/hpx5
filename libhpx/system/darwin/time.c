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
#ifndef __APPLE__
#error The HPX time implementation is configured incorrectly
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/// @file libhpx/platform/darwin/time.c
/// @brief Implements HPX's time interface on Darwin (Mac OS X).
#include <assert.h>
#include <mach/mach_time.h>
#include "hpx/hpx.h"

hpx_time_t hpx_time_now(void) {
  return mach_absolute_time();
}

static uint64_t _diff_ns(hpx_time_t from, hpx_time_t to) {
  static mach_timebase_info_data_t tbi;
  if (tbi.denom == 0)
    (void) mach_timebase_info(&tbi);
  assert(tbi.denom != 0);

  return ((to - from) * tbi.numer/tbi.denom);
}

double hpx_time_diff_us(hpx_time_t from, hpx_time_t to) {
  return _diff_ns(from, to)/1e3;
}

double hpx_time_diff_ms(hpx_time_t from, hpx_time_t to) {
  return _diff_ns(from, to)/1e6;
}

void hpx_time_diff(hpx_time_t start, hpx_time_t end, hpx_time_t *diff) {
  *diff = end - start;
}

double hpx_time_elapsed_us(hpx_time_t from) {
  return hpx_time_diff_us(from, hpx_time_now());
}

double hpx_time_elapsed_ms(hpx_time_t from) {
  return hpx_time_diff_ms(from, hpx_time_now());
}

void hpx_time_elapsed(hpx_time_t start, hpx_time_t *diff) {
  *diff = hpx_time_now() - start;
}

static uint64_t _ns(hpx_time_t time) {
  static mach_timebase_info_data_t tbi;
  if (tbi.denom == 0)
    (void) mach_timebase_info(&tbi);
  assert(tbi.denom != 0);

  return (time * tbi.numer)/tbi.denom;
}

double hpx_time_us(hpx_time_t time) {
  return _ns(time)/1e3;
}

double hpx_time_ms(hpx_time_t time) {
  return _ns(time)/1e6;
}

