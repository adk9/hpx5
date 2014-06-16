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

#include "hpx/hpx.h"
#include "libsync/sync.h"
#include "libsync/lockable_ptr.h"
#include "libhpx/stats.h"

/// ----------------------------------------------------------------------------
/// @file libhpx/scheduler/scheduler.h
/// @brief The internal interface to the scheduler.
///
/// The HPX scheduler is a multithreaded application that provides lighweight
/// threads and local-control-objects (monitor/condition variables). It is
/// designed to work as part of a distributed set of schedulers to support a
/// large-scale, lightweight thread-based application.
/// ----------------------------------------------------------------------------


/// Preprocessor define that tells us if the scheduler is cooperative or
/// preemptive. Unused at this point
#define LIBHPX_SCHEDULER_COOPERATIVE 1
//#define LIBHPX_SCHEDULER_PREEMPTIVE 1


/// Forward declarations
/// @{
struct thread;
struct worker;
struct barrier;
struct cvar;
/// @}

/// ----------------------------------------------------------------------------
/// The scheduler class.
///
/// The scheduler class represents the shared-memory state of the entire
/// scheduling process. It serves as a collection of native worker threads, and
/// a network port, and allows them to communicate with each other and the
/// network.
///
/// It is possible to have multiple scheduler instances active within the same
/// memory space---though it is unclear why we would need or want that at this
/// time---and it is theoretically possible to move workers between schedulers
/// by updating the worker's scheduler pointer and the scheduler's worker
/// table, though all of the functionality that is required to make this work is
/// not implemented.
/// ----------------------------------------------------------------------------
typedef struct scheduler {
  SYNC_ATOMIC(int)     next_id;
  SYNC_ATOMIC(int) next_tls_id;
  int                    cores;
  int                n_workers;
  unsigned int     backoff_max;
  struct worker      **workers;
  struct barrier      *barrier;
  scheduler_stats_t      stats;
} scheduler_t;


/// ----------------------------------------------------------------------------
/// Allocate and initialize a new scheduler.
///
/// @param       cores - the number of processors this scheduler will run on
/// @param     workers - the number of worker threads to start
/// @param  stack_size - the size of the stacks to allocate
/// @param backoff_max - the upper bound for worker backoff
/// @param  statistics - the flag that indicates if statistics are on or off.
/// @returns           - the scheduler object, or NULL if there was an error.
/// ----------------------------------------------------------------------------
HPX_INTERNAL scheduler_t *scheduler_new(int cores, int workers, int stack_size,
                                        unsigned int backoff_max, bool stats);


/// ----------------------------------------------------------------------------
/// Finalize and free the scheduler object.
///
/// The scheduler must already have been shutdown with
/// scheduler_shutdown(). Shutting down a scheduler that is active, or was
/// aborted with scheduler_abort(), results in undefined behavior.
///
/// @param s - the scheduler to free.
/// ----------------------------------------------------------------------------
HPX_INTERNAL void scheduler_delete(scheduler_t *s)
  HPX_NON_NULL(1);


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
HPX_INTERNAL int scheduler_startup(scheduler_t*);


/// ----------------------------------------------------------------------------
/// Stops the scheduler cooperatively.
///
/// This asks all of the threads to shutdown the next time they get a chance to
/// schedule. It is nonblocking.
/// ----------------------------------------------------------------------------
HPX_INTERNAL void scheduler_shutdown(scheduler_t*);


/// ----------------------------------------------------------------------------
/// Join the scheduler at shutdown.
///
/// This will wait until all of the scheduler's worker threads have shutdown.
/// ----------------------------------------------------------------------------
HPX_INTERNAL void scheduler_join(scheduler_t*);


/// ----------------------------------------------------------------------------
/// Stops the scheduler asynchronously.
///
/// This cancels and joins all of the scheduler threads, and then returns. It
/// should only be called by the main thread that called scheduler_startup().
/// ----------------------------------------------------------------------------
HPX_INTERNAL void scheduler_abort(scheduler_t*);


/// ----------------------------------------------------------------------------
/// Spawn a new user-level thread for the parcel.
/// ----------------------------------------------------------------------------
HPX_INTERNAL void scheduler_spawn(hpx_parcel_t *p)
  HPX_NON_NULL(1);


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
/// Wait for an condition.
///
/// This suspends execution of the current user level thread until the condition
/// is signaled. The calling thread must hold the lock. This releases the lock
/// during the wait, but reacquires it before the user-level thread returns.
///
/// scheduler_wait() will call _schedule() and transfer away from the calling
/// thread.
///
/// @param  lco - the lco containing the condition
/// @param cvar - the LCO condition we'd like to wait for
/// ----------------------------------------------------------------------------
HPX_INTERNAL hpx_status_t scheduler_wait(lockable_ptr_t *lock,
                                         struct cvar *cond)
  HPX_NON_NULL(1, 2);


/// ----------------------------------------------------------------------------
/// Signal a condition.
///
/// The calling thread must hold the lock on whatever LCO is protecting the
/// condition. This call is synchronous and all of the waiting threads will be
/// rescheduled (i.e., MESA semantics).
///
/// @param   cvar - the LCO condition we'd like to signal
/// @param status - the status we're signaling
/// ----------------------------------------------------------------------------
HPX_INTERNAL void scheduler_signal(struct cvar *cond, hpx_status_t status)
  HPX_NON_NULL(1);


#endif // LIBHPX_SCHEDULER_H
