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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "hpx.h"

/// ----------------------------------------------------------------------------
/// @file libhpx/time.c
/// @brief Implements HPX's time interface.
/// ----------------------------------------------------------------------------
int
time_init_module(void) {
  return HPX_SUCCESS;
}

void
time_fini_module(void) {
}

int
time_init_thread(void) {
  return HPX_SUCCESS;
}

void
time_fini_thread(void) {
}

hpx_time_t
hpx_time_now(void) {
  return 0;
}

uint64_t
hpx_time_to_us(hpx_time_t time) {
  return time;
}

uint64_t
hpx_time_to_ms(hpx_time_t time) {
  return time;
}
