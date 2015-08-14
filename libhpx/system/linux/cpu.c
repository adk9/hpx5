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

#include <string.h>
#include <unistd.h>
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


int system_get_cores(void) {
  return sysconf(_SC_NPROCESSORS_ONLN);
}

int system_set_affinity(pthread_t thread, int core_id) {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  if (core_id == -1) {
    for (int e = 0; e < system_get_cores(); e++) {
      CPU_SET(e, &cpuset);
    }
  } else {
    CPU_SET(core_id, &cpuset);
  }
  CHECK_LOG(pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset));
  return LIBHPX_OK;
}

int system_set_affinity_group(pthread_t thread, int ncores) {
  cpu_set_t cpu_set;
  CPU_ZERO(&cpu_set);
  CPU_SET(ncores, &cpu_set);
  CHECK_LOG(pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpu_set));
  return LIBHPX_OK;
}

int system_get_affinity_group_size(pthread_t thread, int *ncores) {
  cpu_set_t set;
  CPU_ZERO(&set);
  CHECK_LOG(pthread_getaffinity_np(thread, sizeof(cpu_set_t), &set));
  *ncores = CPU_COUNT(&set);
  return HPX_SUCCESS;
}

void system_get_stack(pthread_t thread, void **base, size_t *size) {
  dbg_assert(base && size);

  pthread_attr_t attr;
  CHECK_ERROR(pthread_getattr_np(thread, &attr));
  CHECK_ERROR(pthread_attr_getstack(&attr, base, size));
}
