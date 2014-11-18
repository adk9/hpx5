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
#ifndef LIBHPX_WORKER_H
#define LIBHPX_WORKER_H

#include <pthread.h>
#include <hpx/hpx.h>
#include <hpx/attributes.h>
#include <libsync/deques.h>
#include <libsync/queues.h>
#include <libhpx/stats.h>

/// Forward declarations.
/// @{
struct scheduler;
/// @}


/// Class representing a worker thread's state.
///
/// Worker threads are "object-oriented" insofar as that goes, but each native
/// thread has exactly one, thread-local worker structure, so the interface
/// doesn't take a "this" pointer and instead grabs the "self" structure using
/// __thread local storage.
///
/// @{
struct worker {
  pthread_t          thread;                    // this worker's native thread
  int                    id;                    // this workers's id
  int               core_id;                    // useful for "smart" stealing
  unsigned int         seed;                    // my random seed
  int                UNUSED;
  void                  *sp;                    // this worker's native stack
  hpx_parcel_t     *current;                    // current thread
  chase_lev_ws_deque_t work;                    // my work
  two_lock_queue_t    inbox;                    // mail sent to me
  volatile int     shutdown;                    // cooperative shutdown flag
  scheduler_stats_t   stats;                    // scheduler statistics
};


/// The main entry function for a scheduler worker thread.
///
/// Each worker will self-assign an ID. IDs are guaranteed to be dense,
/// contiguous integers.
///
/// @param        sched The scheduler instance this worker is associated with.
///
/// @returns            The code passed to worker_shutdown().
void *worker_run(void *sched)
  HPX_INTERNAL HPX_NON_NULL(1);


/// Starts a worker thread associated with a scheduler.
///
/// @param        sched The scheduler instance this worker is associated with.
/// @param        entry The initial parcel for the worker to run.
///
/// @returns            LIBHPX_OK or an error code if the worker failed to
///                     start.
int worker_start(struct scheduler *sched, hpx_parcel_t *entry)
  HPX_INTERNAL HPX_NON_NULL(1);


/// Cooperatively shutdown a worker.
///
/// This does not block.
///
/// @param       worker The worker to shutdown.
/// @param         code An error code to shut down the worker with.
void worker_shutdown(struct worker *worker, int code)
  HPX_INTERNAL HPX_NON_NULL(1);


/// Joins a worker after worker_shutdown().
///
/// This is done separately to allow for cleanup to happen. Also improves
/// shutdown performance because all of the workers can be shutting down and
/// cleaning up in parallel. Workers don't generate values at this point.
///
/// @param       worker The worker to join.
void worker_join(struct worker *worker)
  HPX_INTERNAL HPX_NON_NULL(1);


/// Preemptively shutdown a worker.
///
/// This will leave the (UNIX) process in an undefined state. Only async-safe
/// functions can be used after this routine. This is non-blocking, canceled
/// threads should be joined if you need to wait for them.
///
/// @param       worker The worker to cancel.
void worker_cancel(struct worker *worker)
  HPX_INTERNAL HPX_NON_NULL(1);


#endif // LIBHPX_WORKER_H
