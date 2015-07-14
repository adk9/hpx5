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
#ifndef LIBHPX_WORKER_H
#define LIBHPX_WORKER_H

#include <hpx/hpx.h>
#include <hpx/attributes.h>
#include <libsync/deques.h>
#include <libsync/queues.h>
#include <libhpx/padding.h>
#include <libhpx/stats.h>

/// Forward declarations.
/// @{
struct config;
struct scheduler;
struct ustack;
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
  struct scheduler   *sched;                    // the scheduler instance
  pthread_t          thread;                    // this worker's native thread
  int                    id;                    // this worker's id
  unsigned             seed;                    // my random seed
  int            work_first;                    // this worker's mode
  int               nstacks;                    // count of freelisted stacks
  void                  *sp;                    // this worker's native stack
  hpx_parcel_t     *current;                    // current thread
  struct ustack     *stacks;                    // freelisted stacks
  PAD_TO_CACHELINE(sizeof(struct scheduler*) +
                   sizeof(pthread_t) +
                   sizeof(int) * 4 +
                   sizeof(void *) +
                   sizeof(hpx_parcel_t*) +
                   sizeof(struct ustack*));
  chase_lev_ws_deque_t work;                    // my work
  PAD_TO_CACHELINE(sizeof(chase_lev_ws_deque_t));
  two_lock_queue_t    inbox;                    // mail sent to me
  scheduler_stats_t   stats;                    // scheduler statistics
} HPX_ALIGNED(HPX_CACHELINE_SIZE);

extern __thread struct worker *self;

/// Initialize a worker structure.
///
/// @param            w The worker structure to initialize.
/// @param        sched The scheduler instance.
/// @param           id The worker's id.
/// @param         seed The random seed for this worker.
/// @param    work_size The initial size of the work queue.
///
/// @returns  LIBHPX_OK or an error code
int worker_init(struct worker *w, struct scheduler *sched, int id,
                unsigned seed, unsigned work_size)
  HPX_NON_NULL(1, 2);

/// Finalize a worker structure.
void worker_fini(struct worker *w)
  HPX_NON_NULL(1);

/// Bind a worker structure to the current pthread.
///
/// @param       worker The worker structure for the pthread.
void worker_bind_self(struct worker *worker)
  HPX_NON_NULL(1);

/// Start processing lightweight threads.
int worker_start(void);

/// Creates a worker thread associated with a scheduler.
///
/// The created thread will bind itself to the passed worker, and then start
/// processing lightweight threads.
///
/// @param       worker The worker structure for this worker thread.
/// @param          cfg The configuration object.
///
/// @returns            LIBHPX_OK or an error code if the worker failed to
///                     start.
int worker_create(struct worker *worker, const struct config *cfg)
  HPX_NON_NULL(1);

/// Joins a worker after scheduler_shutdown().
///
/// This is done separately to allow for cleanup to happen. Also improves
/// shutdown performance because all of the workers can be shutting down and
/// cleaning up in parallel. Workers don't generate values at this point.
///
/// @param       worker The worker to join.
void worker_join(struct worker *worker)
  HPX_NON_NULL(1);

/// Preemptively shutdown a worker.
///
/// This will leave the (UNIX) process in an undefined state. Only async-safe
/// functions can be used after this routine. This is non-blocking, canceled
/// threads should be joined if you need to wait for them.
///
/// @param       worker The worker to cancel.
void worker_cancel(struct worker *worker)
  HPX_NON_NULL(1);

/// Check to see if the current worker has enough space for an alloca.
///
/// @param        bytes The number of bytes to allocate.
///
/// @returns            The number of bytes remaining on the stack after the
///                     alloca.
intptr_t worker_can_alloca(size_t bytes);

#endif // LIBHPX_WORKER_H
