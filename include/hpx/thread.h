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

#ifndef HPX_THREAD_H
#define HPX_THREAD_H

/// @addtogroup actions
/// @{

/// @file
/// @brief HPX thread interface
///
/// HPX threads are spawned as a result of hpx_parcel_send() or
/// hpx_parcel_send_sync(), or as a result of an hpx_call() (or variant) which
/// which relies on an implicit parcel send. They may return values to their
/// LCO continuations using the hpx_thread_exit() call, which terminates the
/// thread's execution.

/// Get the target of the current thread.
/// The target of the thread is the destination that a parcel was sent to
/// to spawn the current thread.
///
/// @returns the global address of the thread's target
hpx_addr_t hpx_thread_current_target(void);

/// Get the action that the current thread is executing.
/// @returns the action ID of the current thread
hpx_action_t hpx_thread_current_action(void);

/// Get the address of the continuation for the current thread
/// @returns the address of the current thread's continuation
hpx_addr_t hpx_thread_current_cont_target(void);

/// Get the continuation action for the current thread
/// @returns the continuation action for the current thread
hpx_action_t hpx_thread_current_cont_action(void);

/// Get the process identifier of the current thread
/// @returns the PID for the current thread
hpx_pid_t hpx_thread_current_pid(void);

/// Pause execution and gives other threads the opportunity to be scheduled
void hpx_thread_yield(void);

/// Generates a consecutive new ID for a thread.
///
/// The first time this is called in a lightweight thread, it assigns the thread
/// the next available ID. Each time it's called after that it returns that same
/// id.
///
/// @returns < 0 if there is an error, otherwise a unique, compact id for the
///          calling thread
int hpx_thread_get_tls_id(void);

/// Check to see if the current thread has enough space for an alloca.
///
/// @param        bytes The number of bytes to allocate.
///
/// @returns            The number of bytes remaining on the stack after the
///                     alloca.
intptr_t hpx_thread_can_alloca(size_t bytes);

/// Set a lightweight thread's affinity.
///
/// This isn't generally useful for dataflow-style execution, but can be useful
/// for long running threads where the application programmer has a good idea
/// about the distribution of work that they want.
///
/// This is not a hard guarantee for actual affinity. Various conditions at
/// runtime @i{actually} control where threads execute, including system
/// load. The scheduler will do its best to return a thread to it's assigned
/// locality though. Using affinity badly can cause excessive thread movement
/// and should be used carefully.
///
/// This may block the calling thread in order to induce a context switch. This
/// may be called as frequently as necessary---the most recent affinity will
/// take precedence.
///
/// @param thread_id the scheduler thread id we'd like to set the affinity to,
///                  must be in the range [0, hpx_get_num_threads()).
void hpx_thread_set_affinity(int thread_id);


/// Finish the current thread's execution, sending @p value to the thread's
/// continuation address
///
/// @param size the size of @p value
/// @param value the value to be sent to the thread's continuation address
void _hpx_thread_continue(int nargs, ...)
  HPX_NORETURN;

#define hpx_thread_continue(...)                                        \
  _hpx_thread_continue(__HPX_NARGS(__VA_ARGS__) , ##__VA_ARGS__)

/// Finish the current thread's execution, sending @p v to the thread's
/// continuation address
/// @param v the value to be sent to the thread's continuation
#define HPX_THREAD_CONTINUE(v) hpx_thread_continue(&v, sizeof(v))

/// Finishes the current thread's execution, sending @p value to the thread's
/// continuation address.
///
/// This version gives the application a chance to cleanup for instance, to free
/// the value. After dealing with the continued data, it will run `cleanup(env)`.
///
/// @param cleanup a handler function to be run after the thread ends
/// @param     env an environment to pass to @p cleanup
/// @param    size the size of @p value
/// @param   value the value to be sent to the thread's continuation address
void _hpx_thread_continue_cleanup(void (*cleanup)(void*), void *env,
                                  int nargs, ...)
  HPX_NORETURN;

#define hpx_thread_continue_cleanup(cleanup, env, ...)                  \
  _hpx_thread_continue_cleanup(cleanup, env, __HPX_NARGS(__VA_ARGS__) , ##__VA_ARGS__)

/// Finish the current thread's execution.
///
/// The behavior of this call depends on the @p status parameter, and is
/// equivalent to returning @p status from the action.
///
/// Possible status codes:
/// - HPX_SUCCESS: Normal termination, send a parcel with 0-sized data to
///                the thread's continuation address.
///
/// - HPX_ERROR: Abnormal termination. Terminates execution.
///
/// - HPX_RESEND: Terminate execution, and resend the thread's parcel (NOT
///               the continuation parcel). This can be used for
///               application-level forwarding when hpx_addr_try_pin()
///               fails.
///
/// - HPX_LCO_EXCEPTION: Continue an exception to the continuation address.
///
/// @param status a status to be returned to the function that created this
///        thread
void hpx_thread_exit(int status)
  HPX_NORETURN;

/// @}

#endif
