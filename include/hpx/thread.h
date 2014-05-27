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
#ifndef HPX_THREAD_H
#define HPX_THREAD_H

/// ----------------------------------------------------------------------------
/// HPX thread interface.
///
/// HPX threads are spawned as a result of hpx_parcel_send{_sync}(). The may
/// return values to their LCO continuations using this hpx_thread_exit() call,
/// which terminates the thread's execution.
/// ----------------------------------------------------------------------------
hpx_addr_t hpx_thread_current_target(void);
hpx_addr_t hpx_thread_current_cont(void);
uint32_t   hpx_thread_current_args_size(void);

void hpx_thread_yield(void);

/// ----------------------------------------------------------------------------
/// Generates a consecutive new ID for a thread.
///
/// The first time this is called in a lightweight thread, it assigns the thread
/// the next available ID. Each time it's called after that it returns that same
/// id.
///
/// @returns < 0 if there is an error, otherwise a unique, compact id for the
///          calling thread
/// ----------------------------------------------------------------------------
int hpx_thread_get_tls_id(void);


/// ----------------------------------------------------------------------------
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
/// @param thread_id - the scheduler thread id we'd like to set the affinity to,
///                    must be in the range [0, hpx_get_num_threads()).
/// ----------------------------------------------------------------------------
void hpx_thread_set_affinity(int thread_id);


/// ----------------------------------------------------------------------------
/// Finishes the current thread's execution, sending @p value to the thread's
/// continuation address.
/// ----------------------------------------------------------------------------
void hpx_thread_continue(size_t size, const void *value)
  HPX_NORETURN;
#define HPX_THREAD_CONTINUE(v) hpx_thread_continue(sizeof(v), &v)


/// ----------------------------------------------------------------------------
/// Finishes the current thread's execution, sending @p value to the thread's
/// continuation address.
///
/// This version gives the application a chance to cleanup for instance, to free
/// the value. After dealing with the continued data, it will run cleanup(env).
/// ----------------------------------------------------------------------------
void hpx_thread_continue_cleanup(size_t size, const void *value,
                                 void (*cleanup)(void*), void *env)
  HPX_NORETURN;


/// ----------------------------------------------------------------------------
/// Finish the current thread's execution.
///
/// The behavior of this call depends on the @p status parameter, and is
/// equivalent to returning @p status from the action.
///
///       HPX_SUCCESS: Normal termination, send a parcel with 0-sized data to
///                    the thread's continuation address.
///
///         HPX_ERROR: Abnormal termination. Terminates execution.
///
///        HPX_RESEND: Terminate execution, and resend the thread's parcel (NOT
///                    the continuation parcel). This can be used for
///                    application-level forwarding when hpx_addr_try_pin()
///                    fails.
///
/// HPX_LCO_EXCEPTION: Continue an exception to the continuation address.
/// ----------------------------------------------------------------------------
void hpx_thread_exit(int status)
  HPX_NORETURN;


#endif
