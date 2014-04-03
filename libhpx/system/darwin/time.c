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

/// ----------------------------------------------------------------------------
/// @file libhpx/platform/darwin/time.c
/// @brief Implements HPX's time interface on Darwin (Mac OS X).
/// ----------------------------------------------------------------------------
#include <assert.h>
#include <mach/mach_time.h>
#include "hpx/hpx.h"

hpx_time_t
hpx_time_now(void) {
  return mach_absolute_time();
}

static uint64_t
_elapsed_ns(hpx_time_t from) {
  static mach_timebase_info_data_t tbi;
  if (tbi.denom == 0)
    (void) mach_timebase_info(&tbi);

  hpx_time_t now = hpx_time_now();
  assert(tbi.denom != 0);
  return ((now - from) * tbi.numer/tbi.denom);
}

double
hpx_time_elapsed_us(hpx_time_t from) {
  return _elapsed_ns(from)/1e3;
}

double
hpx_time_elapsed_ms(hpx_time_t from) {
  return _elapsed_ns(from)/1e6;
}

