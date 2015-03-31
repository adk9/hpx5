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

/// @file libhpx/platform/darwin/cpu.c
/// @brief Implements HPX's CPU interface on Darwin (Mac OS X).

#include <sys/types.h>
#include <sys/sysctl.h>
#include <hpx/hpx.h>
#include <libhpx/debug.h>
#include <libhpx/libhpx.h>
#include <libhpx/system.h>

#define CHECK_ERROR(S) do {                                     \
    int e = (S);                                                \
    if (e) {                                                    \
      dbg_error("'"#S"' failed with error %s\n", strerror(e));  \
    }                                                           \
  } while (0)

#define CHECK_LOG(S)  do {                                  \
    int e = (S);                                            \
    if (e) {                                                \
      log("'"#S"' failed with error %s\n", strerror(e));    \
      return LIBHPX_ERROR;                                  \
    }                                                       \
  } while (0)


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

void system_get_stack(pthread_t thread, void **base, size_t *size) {
  dbg_assert(base && size);
  *size = pthread_get_stacksize_np(thread);
  *base = pthread_get_stackaddr_np(thread);
}
