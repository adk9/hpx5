// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2016, Trustees of Indiana University,
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

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>
#include <hpx/hpx.h>
#include <hpx/attributes.h>
#include <libsync/deques.h>
#include <libsync/queues.h>
#include <libhpx/instrumentation.h>
#include <libhpx/padding.h>

/// Forward declarations.
/// @{
struct ustack;
struct network;
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
typedef struct {
  chase_lev_ws_deque_t work;                    // my work
  PAD_TO_CACHELINE(sizeof(chase_lev_ws_deque_t));
} padded_deque_t;

struct worker {
  pthread_t        thread;                      //!< this worker's native thread
  int                  id;                      //!< this worker's id
  unsigned           seed;                      //!< my random seed
  int          work_first;                      //!< this worker's mode
  int             nstacks;                      //!< count of freelisted stacks
  int             yielded;                      //!< used by APEX
  int              active;                      //!< used by APEX
  int         last_victim;                      //!< last successful victim
  int           numa_node;                      //!< this worker's numa node
  void          *profiler;                      //!< reference to the profiler
  void               *bst;                      //!< the block statistics table
  struct network *network;                      //!< reference to the network
  struct logtable   *logs;                      //!< reference to tracer data
  struct scheduler *sched;                      //!< pointer to the scheduler
  hpx_parcel_t    *system;                      //!< this worker's native parcel
  hpx_parcel_t   *current;                      //!< current thread
  struct ustack   *stacks;                      //!< freelisted stacks
  PAD_TO_CACHELINE(sizeof(pthread_t) +
                   sizeof(int) * 8 +
                   sizeof(void*) * 8);
  pthread_mutex_t    lock;                      //!< state lock
  pthread_cond_t  running;                      //!< local condition for sleep
  volatile int      state;                      //!< what state are we in
  volatile int    work_id;                      //!< which queue are we using
  PAD_TO_CACHELINE(sizeof(pthread_mutex_t) +
                   sizeof(pthread_cond_t) +
                   sizeof(int) * 2);
  padded_deque_t   queues[2];                   //!< work and yield queues
  two_lock_queue_t  inbox;                      //!< mail sent to me
};
typedef struct worker worker_t;

_HPX_ASSERT((sizeof(worker_t) & (HPX_CACHELINE_SIZE - 1)) == 0, worker_align);

/// @}

extern __thread worker_t * volatile self;

/// Initialize a worker structure.
///
/// This initializes a worker.
///
/// @param            w The worker structure to initialize.
/// @param        sched The scheduler associated with this worker.
/// @param           id The worker's id.
///
/// @returns  LIBHPX_OK or an error code
void worker_init(worker_t *w, struct scheduler *sched, int id)
  HPX_NON_NULL(1, 2);

/// Finalize a worker structure.
///
/// This will cleanup any queues and free any stacks and parcels associated with
/// the worker. This should only be called once *all* of the workers have been
/// joined so that an _in-flight_ mail message doesn't get missed.
///
/// @param            w The worker structure to finalize.
void worker_fini(worker_t *w)
  HPX_NON_NULL(1);

/// Create a scheduler worker thread.
///
/// This starts an underlying system thread for the scheduler. Assuming the
/// scheduler is stopped the underlying thread will immediately sleep waiting
/// for a run condition.
///
/// @param            w The worker structure to initialize.
/// @param        sched The scheduler associated with this worker.
/// @param           id The worker's id.
///
/// @returns  LIBHPX_OK or an error code
int worker_create(worker_t *w)
  HPX_NON_NULL(1);

/// Join with a scheduler worker thread.
///
/// This will block waiting for the designated worker thread to exit. It should
/// be used before calling worker_fini() on this thread in order to avoid race
/// conditions on the mailbox and scheduling and whatnot.
///
/// @param            w The worker to join (should be active).
void worker_join(worker_t *w)
  HPX_NON_NULL(1);

void worker_stop(worker_t *w)
  HPX_NON_NULL(1);

void worker_start(worker_t *w)
  HPX_NON_NULL(1);

void worker_shutdown(worker_t *w)
  HPX_NON_NULL(1);

/// The thread entry function that the worker uses to start a thread.
///
/// This is the function that sits at the outermost stack frame for a
/// lightweight thread, and deals with dispatching the parcel's action and
/// handling the action's return value.
///
/// It does not return.
///
/// @param            p The parcel to execute.
void worker_execute_thread(hpx_parcel_t *p)
  HPX_NORETURN;

/// Finish processing a worker thread.
///
/// This is the function that handles a return value from a thread. This will be
/// called from worker_execute_thread to terminate processing and does not
/// return.
///
/// @note This is only exposed publicly because it relies on scheduler internals
///       that aren't otherwise visible.
///
/// @param            p The parcel to execute.
/// @param       status The status code that the thread returned with.
void worker_finish_thread(hpx_parcel_t *p, int status)
  HPX_NORETURN;

/// Check to see if the current worker is active.
int worker_is_active(const worker_t *w);

/// Check to see if the current worker is running.
int worker_is_running(const worker_t *w);

#ifdef __cplusplus
}
#endif

#endif // LIBHPX_WORKER_H
