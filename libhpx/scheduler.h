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
#ifndef LIBHPX_SCHEDULER_H
#define LIBHPX_SCHEDULER_H

#include "hpx.h"

#define LIBHPX_SCHEDULER_COOPERATIVE 0
#define LIBHPX_SCHEDULER_PREEMPTIVE 1

struct future;

/// ----------------------------------------------------------------------------
/// Initializes global scheduler data for this address space.
///
/// This performs global initialization for a scheduler instance, and is not
/// idempotent. It must be run before any system thread calls
/// libhpx_sched_startup(), and must be run only once.
///
/// @returns - 0 on success, non-zero on failure
/// ----------------------------------------------------------------------------
HPX_INTERNAL int scheduler_init_module(void);
HPX_INTERNAL void scheduler_fini_module(void);

/// ----------------------------------------------------------------------------
/// Start a scheduler thread.
///
/// This starts a low-level scheduler thread. It will run the scheduling
/// algorithm for user-level threading until libhpx_sched_shutdown() is called
/// by a user-level thread running on top of this scheduler thread, at which
/// point it will return.
///
/// @param  act - An initial HPX user-level action to execute.
/// @param args - Arguments for the intial action.
/// @param size - Number of bytes for args.
/// @returns    - The value provided to libhpx_sched_shutdown().
/// ----------------------------------------------------------------------------
HPX_INTERNAL int scheduler_startup(hpx_action_t act, const void *args, unsigned size);

/// ----------------------------------------------------------------------------
/// Stops scheduling at a locality.
///
/// This can be called by a user-level thread to shutdown a scheduler
/// thread. When it is called, the scheduler will resume the
/// scheduler_startup() routine, and return @p code from it.
///
/// As with longjmp(), this routine does not return to the caller.
///
/// @param code - The return code, to return from scheduler_startup().
/// ----------------------------------------------------------------------------
HPX_INTERNAL void scheduler_shutdown(int code) HPX_NORETURN;

/// ----------------------------------------------------------------------------
/// Spawn a new user-level thread for the parcel.
/// ----------------------------------------------------------------------------
HPX_INTERNAL void scheduler_spawn(hpx_parcel_t *p) HPX_NON_NULL(1);

/// ----------------------------------------------------------------------------
/// Yield a user-level thread.
///
/// This triggers a scheduling event, and possibly selects a new user-level
/// thread to run. If a new thread is selected, this moves the thread into the
/// next local epoch, and also makes the thread available to be stolen within
/// the locality.
/// ----------------------------------------------------------------------------
HPX_INTERNAL void scheduler_yield(void);

/// ----------------------------------------------------------------------------
/// Wait for a future.
///
/// This suspends execution of the current user level thread until the future is
/// signaled. The calling thread must hold the lock on the future. A new
/// user-level thread will be executed. This may generate a new user-level
/// thread that just sits in a scheduling loop if there is nothing else to.
///
/// This releases the lock on future during the wait, but reacquires it before
/// returning.
/// ----------------------------------------------------------------------------
HPX_INTERNAL void scheduler_wait(struct future *future) HPX_NON_NULL(1);

HPX_INTERNAL void scheduler_signal(struct future *f, const void *val, int size)
  HPX_NON_NULL(1);

/// ----------------------------------------------------------------------------
/// Exit a user level thread.
///
/// @param parcel - the parcel bound to the current stack (will be released)
/// ----------------------------------------------------------------------------
HPX_INTERNAL void scheduler_exit(hpx_parcel_t *parcel)
  HPX_NON_NULL(1) HPX_NORETURN;

#endif // LIBHPX_SCHEDULER_H
