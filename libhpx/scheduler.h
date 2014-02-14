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

/// ----------------------------------------------------------------------------
/// Initializes global scheduler data for this address space.
///
/// This performs global initialization for a scheduler instance, and is not
/// idempotent. It must be run before any system thread calls
/// libhpx_sched_startup(), and must be run only once.
///
/// @returns - 0 on success, non-zero on failure
/// ----------------------------------------------------------------------------
HPX_INTERNAL int scheduler_init(void);

/// ----------------------------------------------------------------------------
/// Initializes thread-local scheduler data for this address space.
///
/// @returns - 0 on success, non-zero on failure
/// ----------------------------------------------------------------------------
HPX_INTERNAL int scheduler_init_thread(void);

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
/// Yield a user-level thread.
///
/// The core of the cooperative scheduler. A user-level thread can call
/// libhpx_sched_yield() in order to suspend execution of the current thread,
/// and engage the scheduling algorithm.
///
/// libhpx_sched_yield() might be call proactively by libhpx code in order to
/// ensure that scheduling occurs periodically. When this happens, the thread is
/// simply resumed again in the next epoch. In this situation, @p n should be
/// 0, and @p lco should be NULL.
///
/// libhpx_sched_yield() might also be called explicitly by the HPX application
/// layer through the hpx_thread_wait() or hpx_thread_wait_all() routine, in
/// which case the application is attempting to wait for an LCO, or set of LCOs,
/// to be set before proceeding. The scheduler is responsible for arranging this
/// to work correctly and the thread will become blocked until all of the LCOs
/// designated in @p lco are signaled.
///
/// @param    n - The number of LCOs that the thread wants to wait for.
/// @param lcos - The global addresses for the LCOs that the thread wants to
///               wait for.
/// ----------------------------------------------------------------------------
HPX_INTERNAL void scheduler_yield(unsigned n, hpx_addr_t lcos[n]);


#endif // LIBHPX_SCHEDULER_H
