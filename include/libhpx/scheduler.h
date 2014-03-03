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

struct lco;


/// ----------------------------------------------------------------------------
/// Starts the scheduler.
///
/// This starts all of the low-level scheduler threads. After this call, threads
/// can be spawned using the scheduler_spawn() routine. Parcels for this queue
/// may come from the network, or from the main thread.
///
/// @param config - the configuration object
/// @returns      - non-0 if there is a startup problem
/// ----------------------------------------------------------------------------
HPX_INTERNAL int scheduler_startup(const hpx_config_t *config);


/// ----------------------------------------------------------------------------
/// Stops the scheduler.
///
/// This cancels and joins all of the scheduler threads, and then returns. It
/// should only be called by the main thread that called scheduler_startup().
/// ----------------------------------------------------------------------------
HPX_INTERNAL void scheduler_shutdown(void);


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
/// Wait for an LCO.
///
/// This suspends execution of the current user level thread until the LCO is
/// signaled. The calling thread must hold the lock on the LCO. This releases
/// the lock on the lco during the wait, but reacquires it before the user-level
/// thread returns.
///
/// scheduler_wait() will call _schedule() and transfer away from the calling
/// thread.
/// ----------------------------------------------------------------------------
HPX_INTERNAL void scheduler_wait(struct lco *lco) HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// Signal an LCO.
///
/// The calling thread must hold the lock on the LCO. This will modify the LCO
/// in the following way.
///
///   1) it will mark the LCO as SET
///   2) it will set the LCO's wait queue to NULL
///   3) it will release the LCO's lock
///   4) each of the previously waiting threads will be rescheduled
/// ----------------------------------------------------------------------------
HPX_INTERNAL void scheduler_signal(struct lco *lco) HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// Exit a user level thread.
///
/// @param parcel - the parcel bound to the current stack (will be released)
/// ----------------------------------------------------------------------------
HPX_INTERNAL void scheduler_exit(hpx_parcel_t *parcel)
  HPX_NON_NULL(1) HPX_NORETURN;


/// ----------------------------------------------------------------------------
/// Get the parcel for the current thread.
/// ----------------------------------------------------------------------------
HPX_INTERNAL hpx_parcel_t *scheduler_current_parcel(void);


/// ----------------------------------------------------------------------------
/// Get the target for the current thread.
/// ----------------------------------------------------------------------------
HPX_INTERNAL hpx_addr_t scheduler_current_target(void);


#endif // LIBHPX_SCHEDULER_H
