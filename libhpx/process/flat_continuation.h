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
#ifndef LIBHPX_PROCESS_FLAT_CONTINUATION_H
#define LIBHPX_PROCESS_FLAT_CONTINUATION_H

/// @file libhpx/process/flat_continuation.h
///
/// This header defines the interface to the local component of a distributed
/// continuation. Essentially, threads are enqueued in their locality and the
/// continuation is distributed through a broadcast operation.

/// Forward declare the parcel type as a continuation type.
/// @{
struct hpx_parcel;
/// @}

/// Allocate a new local continuation element.
void *flat_continuation_new(void);

/// Delete a continuation element.
void flat_continuation_delete(void *obj);

/// Enqueue a continuation locally.
void flat_continuation_wait(void *obj, struct hpx_parcel *p);

/// Trigger the set of waiting continuations with the passed value.
void flat_continuation_trigger(void *obj, const void *value, size_t bytes);

#endif // LIBHPX_PROCESS_FLAT_CONTINUATION_H
