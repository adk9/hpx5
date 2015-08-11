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

/// @file libhpx/platform/linux/time.c
/// @brief Implements HPX's time interface on linux.
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <hpx/hpx.h>
#include <libhpx/debug.h>

static hpx_time_t _beginning_of_time;

hpx_time_t hpx_time_now(void) {
  hpx_time_t time;
  if (clock_gettime(CLOCK_MONOTONIC, &time)) {
    dbg_error("system: hpx_time_now() failed to get time.\n");
  }
  return time;
}

static double _diff_ns(hpx_time_t from, hpx_time_t to) {
  return (double)(((to.tv_sec - from.tv_sec) * 1e9) + (to.tv_nsec - from.tv_nsec));
}

double hpx_time_diff_us(hpx_time_t from, hpx_time_t to) {
  return _diff_ns(from, to)/1e3;
}

double hpx_time_diff_ms(hpx_time_t from, hpx_time_t to) {
  return _diff_ns(from, to)/1e6;
}

void hpx_time_diff(hpx_time_t start, hpx_time_t end, hpx_time_t *diff) {
  if (end.tv_nsec < start.tv_nsec) {
    diff->tv_sec = end.tv_sec - start.tv_sec - 1;
    diff->tv_nsec = (1e9 + end.tv_nsec) - start.tv_nsec;
  } else {
    diff->tv_sec = end.tv_sec - start.tv_sec;
    diff->tv_nsec = end.tv_nsec - start.tv_nsec;
  }
}

double hpx_time_elapsed_us(hpx_time_t from) {
  return hpx_time_diff_us(from, hpx_time_now());
}

double hpx_time_elapsed_ms(hpx_time_t from) {
  return hpx_time_diff_ms(from, hpx_time_now());
}

void hpx_time_elapsed(hpx_time_t start, hpx_time_t *diff) {
  hpx_time_diff(start, hpx_time_now(), diff);
}

static double _ns(hpx_time_t time) {
  return (time.tv_sec * 1e9) + time.tv_nsec;
}

double hpx_time_us(hpx_time_t time) {
  return _ns(time)/1e3;
}

double hpx_time_ms(hpx_time_t time) {
  return _ns(time)/1e6;
}

uint64_t hpx_time_elapsed_ns(hpx_time_t from, hpx_time_t to) {
  return (uint64_t)(((to.tv_sec - from.tv_sec) * 1e9) + (to.tv_nsec - from.tv_nsec));
}

uint64_t hpx_time_to_ns(hpx_time_t t) {
  return hpx_time_elapsed_ns(_beginning_of_time, t);
}

hpx_time_t hpx_time_construct(unsigned long s, unsigned long ns) {
  hpx_time_t t;
  t.tv_sec = s;
  t.tv_nsec = ns;
  return t;
}

hpx_time_t hpx_time_point(hpx_time_t time, hpx_time_t duration) {
  hpx_time_t t;
  long ns = time.tv_nsec + duration.tv_nsec;
  if (ns > 1e9) {
    t.tv_nsec = ns % 1000000000;
    t.tv_sec = time.tv_nsec + duration.tv_nsec + 1;
  }
  else {
    t.tv_nsec = ns;
    t.tv_sec = time.tv_nsec + duration.tv_nsec;
  }

  return t;
}

void libhpx_time_start() {
  _beginning_of_time = hpx_time_now();
}

hpx_time_t libhpx_beginning_of_time() {
  return _beginning_of_time;
}
