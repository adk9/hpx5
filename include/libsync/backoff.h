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
#ifndef LIBSYNC_BACKOFF_H
#define LIBSYNC_BACKOFF_H

#include "hpx/attributes.h"
#include "sync.h"

SYNC_INTERNAL void sync_backoff(unsigned int i)
  HPX_NOINLINE;


SYNC_INTERNAL void sync_backoff_exp_r(unsigned int *prev)
  HPX_NOINLINE HPX_NON_NULL(1);


#endif // LIBSYNC_BACKOFF_H
