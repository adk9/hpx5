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
#include "hpx.h"

hpx_time_t
hpx_time_now(void) {
  hpx_time_t time;
  if (clock_gettime(CLOCK_MONOTONIC, &time)) {
    fprintf(stderr, "hpx_time_now() failed to get time.\n");
  }
  return time;
}

static double
_elapsed_ns(hpx_time_t from) {
  hpx_time_t now = hpx_time_now();
  return (double)(((now.tv_sec - from.tv_sec) * 1e9) + (now.tv_nsec - from.tv_nsec));
}

double
hpx_time_elapsed_us(hpx_time_t from) {
  return _elapsed_ns(from)/1e3;
}

double
hpx_time_elapsed_ms(hpx_time_t from) {
  return _elapsed_ns(from)/1e6;
}
