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
#ifndef LIBHPX_SYSTEM_H
#define LIBHPX_SYSTEM_H

#include <pthread.h>
#include "hpx/attributes.h"

int system_get_cores(void)
  HPX_INTERNAL;

int system_set_affinity(pthread_t thread, int core_id)
  HPX_INTERNAL;

int system_set_affinity_group(pthread_t thread, int ncores)
  HPX_INTERNAL;

#endif // LIBHPX_SYSTEM_H
