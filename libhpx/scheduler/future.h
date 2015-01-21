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
#ifndef LIBHPX_SCHEDULER_FUTURE_H
#define LIBHPX_SCHEDULER_FUTURE_H

/// This exposes the future-set interface as a hack for netfuture.c

void lco_future_set(lco_t *lco, int size, const void *from)
  HPX_INTERNAL;

#endif
