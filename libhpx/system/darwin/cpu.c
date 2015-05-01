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

#include <inttypes.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <hpx/hpx.h>
#include <libhpx/debug.h>
#include <libhpx/libhpx.h>
#include <libhpx/system.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <pthread.h>

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

int system_get_affinity_group_size(pthread_t thread, int *ncores) {
  // First get affinity tag and check whether it is null
  // (THREAD_AFFINITY_TAG_NULL). If it is not THREAD_AFFINITY_TAG_NULL
  // then thread is associated with a L2 cache. Then find the logical
  // processors associated with L2 cache.  Ref:
  // https://developer.apple.com/library/mac/releasenotes/Performance/RN-AffinityAPI/
  mach_msg_type_number_t count = THREAD_AFFINITY_POLICY_COUNT;
  
  thread_affinity_policy_data_t policy;
  boolean_t b = FALSE;
  
  int kr = thread_policy_get(pthread_mach_thread_np(thread),
			     THREAD_AFFINITY_POLICY, (thread_policy_t)&policy,
                             &count, &b);

  if (kr != KERN_SUCCESS) {
    return HPX_ERROR;
  }

  // check whether affinity tag is null. If it is not null then thread
  // is associated with a L2 cache. Then find the number of logical
  // processors associated with L2 cache.
  if (policy.affinity_tag != THREAD_AFFINITY_TAG_NULL) { 
    // Trying to get the logical processors associated with a L2 cache
    size_t size;
    if (!sysctlbyname("hw.cacheconfig", NULL, &size, NULL, 0)) {
      unsigned n = size / sizeof(uint32_t);
      uint64_t cacheconfig[n];

      if (!sysctlbyname("hw.cacheconfig", cacheconfig, &size, NULL, 0)) {
	if (n >= 2) {
	  *ncores = (int)cacheconfig[2];
        } else {
	  return HPX_ERROR;
        }
      }
    }
  } else {  
    *ncores = 1;
  }
  return HPX_SUCCESS;
}
