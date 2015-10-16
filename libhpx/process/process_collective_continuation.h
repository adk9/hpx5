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
#ifndef LIBHPX_PROCESS_PROCESS_COLLECTIVE_CONTINUATION_H
#define LIBHPX_PROCESS_PROCESS_COLLECTIVE_CONTINUATION_H

/// @file libhpx/process/process_collective_continuation.h
///
/// A dynamic broadcast array is a cyclic array that can be used by collective
/// operations to perform a quasi-hierarchical broadcast of a reduced or
/// collected result.
///
/// Basically, the array is a cyclic array that a collective participant will
/// register a continuation with each iteration, before joining the
/// collective. When the collective completes, all of the distributed
/// continuations will be triggered with the collective result.

#include <hpx/hpx.h>

hpx_addr_t process_collective_continuation_new(size_t bytes, hpx_addr_t gva);

hpx_addr_t process_collective_continuation_append(hpx_addr_t gva, size_t bytes,
                                                  hpx_addr_t c_action,
                                                  hpx_addr_t c_target);

int process_collective_continuation_set_lsync(hpx_addr_t gva, size_t bytes,
                                              const void *buffer);

#endif // LIBHPX_PROCESS_PROCESS_COLLECTIVE_CONTINUATION_H
