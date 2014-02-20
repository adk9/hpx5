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
#ifndef LIBHPX_FUTURE_H
#define LIBHPX_FUTURE_H

/// ----------------------------------------------------------------------------
/// @file future.h
/// Declares the future structure, and its internal interface.
/// ----------------------------------------------------------------------------
#include <stdint.h>
#include "hpx.h"

/// ----------------------------------------------------------------------------
/// Forward declare the stack type for the wait queue.
/// ----------------------------------------------------------------------------
struct ustack;

/// ----------------------------------------------------------------------------
/// Future structure.
///
/// Futures basically implement two things. The first is a scheduler wait
/// queue. Threads can wait for futures to be signaled by putting their stacks
/// into the queue.
///
/// The second is a set-able value. The value is set by the signaler, and is
/// retrieved by all of the waiters after the signal.
/// ----------------------------------------------------------------------------
typedef struct future future_t;
struct future {
  struct ustack *waitq;
  void *value;
};

/// ----------------------------------------------------------------------------
/// Signal a future.
///
/// This will lock the future, copy the @p data, set the state to TRIGGERED and
/// unlock the future, and return the list of waiting threads (i.e., the stack
/// of waiting stacks).
///
/// @nb It is only called from the scheduler at this point. All of the threads
///     in the returned stack must be notified that a future that they are
///     waiting for has been signaled.
///
/// @param future - the future to signal
/// @param   data - the data to set the future's value to, WILL BE COPIED
/// @param   size - the number of bytes of @p data
/// @returns      - a stack of threads that were waiting for this signal
/// ----------------------------------------------------------------------------
HPX_INTERNAL struct ustack *future_signal(future_t *future, const void *data,
                                          int size)
  HPX_NON_NULL(1);

/// ----------------------------------------------------------------------------
///
/// ----------------------------------------------------------------------------
HPX_INTERNAL void future_wait(future_t *future, struct ustack *stack)
  HPX_NON_NULL(1, 2);

HPX_INTERNAL void future_lock(future_t *future) HPX_NON_NULL(1);

#endif // LIBHPX_FUTURE_H
