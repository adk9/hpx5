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
#error The HPX CPU implementation is configured incorrectly
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/sysctl.h>
#include "libhpx/debug.h"
#include "libhpx/system.h"
#include "hpx/hpx.h"

/// ----------------------------------------------------------------------------
/// @file libhpx/platform/darwin/cpu.c
/// @brief Implements HPX's CPU interface on Darwin (Mac OS X).
/// ----------------------------------------------------------------------------

int system_get_cores(void)
{
  int cores;
  size_t length = sizeof(cores);
  sysctlbyname("hw.ncpu", &cores, &length, NULL, 0);
  return cores;
}

int system_set_affinity(pthread_t thread, int core_id) {
  // there's no good way to do this on darwin yet, so we do nothing.
  log("thread binding not supported on darwin.\n");
  return HPX_SUCCESS;
}

int system_set_affinity_group(pthread_t thread, int ncores) {
  log("thread binding not supported on darwin.\n");
  return HPX_SUCCESS;
}
