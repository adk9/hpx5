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
struct lco;
struct thread;
struct network;
struct worker;
struct barrier;
typedef struct scheduler scheduler_t;
typedef int (*process_map_t)(scheduler_t *, int);
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
struct scheduler {
  process_map_t      pmap;
  int               cores;
  struct network *network;
  int           n_workers;
  struct worker **workers;
  struct barrier *barrier;
};


/// ----------------------------------------------------------------------------
/// Allocate and initialize a new scheduler.
///
/// @param    network - a network for remote operations
/// @param      cores - the number of processors this scheduler will run on
/// @param    workers - the number of worker threads to start
/// @param stack_size - the size of the stacks to allocate
/// @param       pmap - a function to map worker id to processor
/// @returns          - the scheduler object, or NULL if there was an error.
/// ----------------------------------------------------------------------------
HPX_INTERNAL scheduler_t *scheduler_new(struct network *network, int cores,
                                        int workers, int stack_size,
                                        process_map_t pmap)
  HPX_NON_NULL(1);


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
/// schedule. It is both cooperative and blocking, and may not return if there
/// is a misbehaving HPX lightweight thread that does not return to the
/// scheduler.
///
/// @todo This should be non-blocking, either through a timeout or through a
///       test_shutdown() routine.
/// ----------------------------------------------------------------------------
HPX_INTERNAL void scheduler_shutdown(scheduler_t*);


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
HPX_INTERNAL void scheduler_wait(struct lco *lco)
  HPX_NON_NULL(1);


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
HPX_INTERNAL void scheduler_signal(struct lco *lco)
  HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// Exit a user level thread.
///
/// Currently takes the parcel pointer for the current thread as a performance
/// optimization, since everywhere this is used we already have the parcel. This
/// isn't necessary, as we can always extract the current parcel using
/// scheduler_current_parcel().
///
/// @param parcel - the parcel bound to the current stack (will be released)
/// ----------------------------------------------------------------------------
HPX_INTERNAL void scheduler_exit(hpx_parcel_t *parcel)
  HPX_NON_NULL(1) HPX_NORETURN;


/// ----------------------------------------------------------------------------
/// Get the current thread.
/// ----------------------------------------------------------------------------
HPX_INTERNAL struct thread *scheduler_current_thread(void);


/// ----------------------------------------------------------------------------
/// Get the current parcel.
/// ----------------------------------------------------------------------------
HPX_INTERNAL hpx_parcel_t *scheduler_current_parcel(void)
  HPX_RETURNS_NON_NULL;


#endif // LIBHPX_SCHEDULER_H
