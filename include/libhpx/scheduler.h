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

#ifndef LIBHPX_SCHEDULER_H
#define LIBHPX_SCHEDULER_H

/// @file libhpx/scheduler/scheduler.h
/// @brief The internal interface to the scheduler.
///
/// The HPX scheduler is a multithreaded application that provides lighweight
/// threads and local-control-objects (monitor/condition variables). It is
/// designed to work as part of a distributed set of schedulers to support a
/// large-scale, lightweight thread-based application.

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>
#include <hpx/hpx.h>
#include <libsync/queues.h>
#include <libhpx/worker.h>

/// Preprocessor define that tells us if the scheduler is cooperative or
/// preemptive. Unused at this point.
/// @{
#define LIBHPX_SCHEDULER_COOPERATIVE 1
//#define LIBHPX_SCHEDULER_PREEMPTIVE 1
/// @}

/// Forward declarations
/// @{
struct config;
struct cvar;
/// @}

/// Scheduler states.
/// @{
enum {
  SCHED_SHUTDOWN,
  SCHED_STOP,
  SCHED_RUN,
};
/// @}

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
struct scheduler {
  pthread_mutex_t     lock;                     //!< lock for running condition
  pthread_cond_t   stopped;                     //!< the running condition
  volatile int       state;                     //!< the run state
  volatile int next_tls_id;                     //!< lightweight thread ids
  int            n_workers;                     //!< total number of workers
  int             n_target;                     //!< target number of workers
  volatile int    n_active;                     //!< active number of workers
  volatile int        code;                     //!< the exit code
  PAD_TO_CACHELINE(sizeof(pthread_mutex_t) +
                   sizeof(pthread_cond_t) +
                   sizeof(int) * 6);            //!< padding to align workers
  worker_t         workers[];                   //!< array of worker data
};
typedef struct scheduler scheduler_t;

/// Allocate and initialize a new scheduler.
///
/// @param       config The configuration object.
///
/// @returns            The scheduler object, or NULL if there was an error.
scheduler_t *scheduler_new(const struct config *config)
  HPX_MALLOC;

/// Finalize and free the scheduler object.
///
/// The scheduler must already have been shutdown with
/// scheduler_shutdown(). Shutting down a scheduler that is active results in
/// undefined behavior.
///
/// @param    scheduler The scheduler to free.
void scheduler_delete(scheduler_t *scheduler);

/// Restart the scheduler.
///
/// This resumes all of the low-level scheduler threads that were
/// suspended at the end of the previous execution of hpx_run. It will block
/// until the run epoch is terminated, at which point it will return the
/// status.
///
/// @param        sched The scheduler to restart.
/// @param      startup The initial lightweight thread.
/// @param         spmd True if every locality should run the startup action.
int scheduler_start(scheduler_t *sched, hpx_parcel_t *startup, int spmd)
  HPX_NON_NULL(1, 2);

/// Suspend the scheduler cooperatively.
///
/// This asks all of the threads to stop running.
///
/// @param    scheduler The scheduler to stop.
/// @param         code The code to return .
void scheduler_stop(scheduler_t *scheduler, int code)
  HPX_NON_NULL(1);

/// Check to see if the scheduler is running.
///
/// @param    scheduler The scheduler to check.
///
/// @returns            True if the scheduler is stopped.
int scheduler_is_stopped(const scheduler_t *scheduler)
  HPX_NON_NULL(1);

/// Get a worker by id.
worker_t *scheduler_get_worker(scheduler_t *sched, int id)
  HPX_NON_NULL(1);

/// Spawn a new user-level thread for the parcel on the specified
/// worker thread.
///
/// @param         p The parcel to spawn.
/// @param    thread The worker thread id to spawn the parcel on.
void scheduler_spawn_at(hpx_parcel_t *p, int thread)
  HPX_NON_NULL(1);

/// Spawn a new user-level thread for the parcel.
///
/// @param    p The parcel to spawn.
void scheduler_spawn(hpx_parcel_t *p)
  HPX_NON_NULL(1);

/// Yield a user-level thread.
///
/// This triggers a scheduling event, and possibly selects a new user-level
/// thread to run. If a new thread is selected, this moves the thread into the
/// next local epoch, and also makes the thread available to be stolen within
/// the locality.
void scheduler_yield(void);

/// Suspend the execution of the current thread.
///
/// This suspends the execution of the current lightweight thread. It must be
/// explicitly resumed in the future. The continuation @p f(p, @p env) is run
/// synchronously after the thread has been suspended but before a new thread is
/// run. This allows the lightweight thread to perform "safe" synchronization
/// where @p f will trigger a resume operation on the current thread, and we
/// don't want to miss that signal or possibly have our thread stolen before it
/// is checkpointed. The runtime passes the parcel we transferred away from to
/// @p as the first parameter.
///
/// This will find a new thread to execute, and will effect a context switch. It
/// is not possible to immediately switch back to the current thread, even if @p
/// f makes it runnable, because thread selection is performed prior to the
/// execution of @p f, and the current thread is not a valid option at that
/// point.
///
/// The continuation @p f, and the environment @p env, can be on the stack.
///
/// @param            f A continuation to run after the context switch.
/// @param          env The environment to pass to the continuation @p f.
/// @param        block A flag indicating if it is okay to block before running
///                     @p f.
void scheduler_suspend(void (*f)(hpx_parcel_t *, void*), void *env);

/// Wait for an condition.
///
/// This suspends execution of the current user level thread until the condition
/// is signaled. The calling thread must hold the lock. This releases the lock
/// during the wait, but reacquires it before the user-level thread returns.
///
/// scheduler_wait() will call _schedule() and transfer away from the calling
/// thread.
///
/// @param         lock The tatas lock protecting the condition.
/// @param          con The condition we'd like to wait for.
///
/// @returns            LIBHPX_OK or an error
hpx_status_t scheduler_wait(void *lock, struct cvar *con)
  HPX_NON_NULL(1, 2);

/// Signal a condition.
///
/// The calling thread must hold the lock protecting the condition. This call is
/// synchronous (MESA style) and one waiting thread will be woken up.
///
/// @param         cond The condition we'd like to signal.
void scheduler_signal(struct cvar *cond)
  HPX_NON_NULL(1);

/// Signal a condition.
///
/// The calling thread must hold the lock protecting the condition. This call is
/// synchronous (MESA style) and all waiting threads will be woken up.
///
/// @param         cvar The condition we'd like to signal.
void scheduler_signal_all(struct cvar *cvar)
  HPX_NON_NULL(1);

/// Signal an error condition.
///
/// The calling thread must hold the lock on protecting the condition. This call
/// is synchronous (MESA style) and all of the waiting threads will be woken up,
/// with the return value of HPX_LCO_ERROR. The user-supplied error can be
/// retrieved from the condition.
///
/// @param         cvar The condition we'd like to signal an error on.
/// @param         code The error code to set in the condition.
void scheduler_signal_error(struct cvar *cvar, hpx_status_t code)
  HPX_NON_NULL(1);

/// Get the parcel bound to the current executing thread.
hpx_parcel_t *scheduler_current_parcel(void);

#ifdef __cplusplus
}
#endif

#endif // LIBHPX_SCHEDULER_H
