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
#ifndef LIBHPX_NETWORK_PROGRESS_H
#define LIBHPX_NETWORK_PROGRESS_H

#include <pthread.h>
#include <hpx/attributes.h>

struct locality;

pthread_t progress_start(struct locality *locality)
  HPX_INTERNAL HPX_NON_NULL(1);

void progress_stop(pthread_t thread)
  HPX_INTERNAL;

#endif // LIBHPX_NETWORK_PROGRESS_H
