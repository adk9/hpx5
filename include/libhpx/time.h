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
#ifndef LIBHPX_TIME_H
#define LIBHPX_TIME_H

/// Initialize internal clocks
/// This is presently used mainly to enable hpx_time_to_ns(),
/// but could be used to provide other functionality.
void libhpx_time_start();

#endif // LIBHPX_UTILS_H
