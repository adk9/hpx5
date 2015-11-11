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

#include <pthread.h>
#include <hpx/hpx.h>
#include <hpx/attributes.h>
#include <libsync/deques.h>
#include <libsync/queues.h>
#include <libhpx/padding.h>
#include <libhpx/stats.h>

/// Forward declarations.
/// @{
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
typedef struct {
  pthread_t          thread;                    // this worker's native thread
  int                    id;                    // this worker's id
  unsigned             seed;                    // my random seed
  int            work_first;                    // this worker's mode
  int               nstacks;                    // count of freelisted stacks
  int               yielded;                    // used by APEX
  int                active;                    // used by APEX scheduler throttling
  hpx_parcel_t      *system;                    // this worker's native parcel
  hpx_parcel_t     *current;                    // current thread
  struct ustack     *stacks;                    // freelisted stacks
  PAD_TO_CACHELINE(sizeof(pthread_t) +
                   sizeof(int) * 6 +
                   sizeof(hpx_parcel_t*) * 2 +
                   sizeof(struct ustack*));
  chase_lev_ws_deque_t work;                    // my work
  PAD_TO_CACHELINE(sizeof(chase_lev_ws_deque_t));
  two_lock_queue_t    inbox;                    // mail sent to me
  libhpx_stats_t      stats;                    // per-worker statistics
  int           last_victim;                    // last successful victim
  int             numa_node;                    // this worker's numa node
  void            *profiler;                    // worker maintains a
                                                // reference to its profiler
} worker_t HPX_ALIGNED(HPX_CACHELINE_SIZE);

extern __thread worker_t * volatile self;

/// Initialize a worker structure.
///
/// @param            w The worker structure to initialize.
/// @param           id The worker's id.
/// @param         seed The random seed for this worker.
/// @param    work_size The initial size of the work queue.
///
/// @returns  LIBHPX_OK or an error code
int worker_init(worker_t *w, int id, unsigned seed, unsigned work_size)
  HPX_NON_NULL(1);

/// Finalize a worker structure.
void worker_fini(worker_t *w)
  HPX_NON_NULL(1);

/// Start processing lightweight threads.
int worker_start(void);

/// Reset a worker thread.
void worker_reset(worker_t *w)
  HPX_NON_NULL(1);

/// Check to see if the current worker should be active.
int worker_is_active(void);

/// Check to see if the current worker should shut down completely.
int worker_is_shutdown(void);

///  This is a custom thread barrier which will be disabled
///  automatically if a locality-wide shutdown is issued.  This
///  generally happens at hpx_finalize() and will enable all waiting
///  threads to come out of the main schedule loop().
int worker_wait(void);

#endif // LIBHPX_WORKER_H
