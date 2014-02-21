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

HPX_INTERNAL int future_init_module(void);
HPX_INTERNAL void future_fini_module(void);

HPX_INTERNAL int future_init_thread(void);
HPX_INTERNAL void future_fini_module(void);

/// ----------------------------------------------------------------------------
/// Forward declare the thread type for the wait queue.
/// ----------------------------------------------------------------------------
struct thread;

/// ----------------------------------------------------------------------------
/// Future structure.
///
/// Futures basically implement two things. The first is a scheduler wait
/// queue. Threads can wait for futures to be signaled by putting their threads
/// into the queue.
///
/// The second is a set-able value. The value is set by the signaler, and is
/// retrieved by all of the waiters after the signal.
/// ----------------------------------------------------------------------------
typedef struct future future_t;
struct future {
  struct thread *waitq;                         // packed pointer stack
  void *value;
};

/// ----------------------------------------------------------------------------
/// Sets a future's value.
///
/// This will lock the future, copy the @p data, mark it as set, unlock the
/// future, and return the list of waiting threads (i.e., the stack of waiting
/// threads).
///
/// @nb It is only called from the scheduler at this point. All of the threads
///     in the returned stack must be notified that a future that they are
///     waiting for has been signaled.
///
/// @param    f - the future to signal
/// @param data - the data to set the future's value to, WILL BE COPIED
/// @param size - the number of bytes of @p data
/// @returns    - a stack of threads that were waiting for this signal
/// ----------------------------------------------------------------------------
HPX_INTERNAL struct thread *future_set(future_t *f, const void *data, int size)
  HPX_NON_NULL(1);

/// ----------------------------------------------------------------------------
/// Acquire the future's lock.
///
/// Used by the scheduler to reacquire the future after a wait.
/// ----------------------------------------------------------------------------
HPX_INTERNAL void future_lock(future_t *future) HPX_NON_NULL(1);

#endif // LIBHPX_FUTURE_H
