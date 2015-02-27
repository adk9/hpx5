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
#ifndef HPX_RUNTIME_H
#define HPX_RUNTIME_H

#include "hpx/attributes.h"
#include "hpx/builtins.h"
#include "hpx/action.h"

/// @file
/// @brief HPX system interface.
/// hpx_init() initializes the scheduler, network, and locality
///
/// hpx_run() is called from the native thread after hpx_init() and action
/// registration is complete, in order
///
/// hpx_abort() is called from an HPX lightweight thread to terminate scheduler
/// execution asynchronously
///
/// hpx_shutdown() is called from an HPX lightweight thread to terminate
/// scheduler execution


/// Initializes the HPX runtime.
///
/// This must be called before other HPX functions.  hpx_init() initializes the
/// scheduler, network, and locality and should be called before any other HPX
/// functions.
///
/// @param argc   count of command-line arguments
/// @param argv   array of command-line arguments
/// @returns      HPX_SUCCESS on success
int hpx_init(int *argc, char ***argv);


/// Start the HPX runtime, and run a given action.
///
/// This creates an HPX "main" process, and calls the given action @p
/// entry in the context of this process. The @p entry action is
/// invoked only on the root locality. On termination, it deletes the
/// main process and returns the status returned by hpx_shutdown()
///
/// hpx_run finalizes action registration, starts up any scheduler and
/// native threads that need to run, and transfers all control into
/// the HPX scheduler, beginning execution of the top action in the
/// scheduler queue.
///
/// The scheduler queue could be empty, in which case the entire
/// scheduler instance is running, waiting for a successful
/// inter-locality steal operation (if that is implemented) or a
/// network parcel.
///
/// @param entry an action to execute, or HPX_ACTION NULL to wait for an
///              incoming parcel or a inter-locality steal (if implemented)
/// @param  args arguments to pass to @p entry
/// @param  size the size of @p args
/// @returns     the status code passed to hpx_shutdown() upon
///              termination.
int    _hpx_run(hpx_action_t *entry, int nargs, ...);
#define hpx_run(entry, ...) _hpx_run(entry, __HPX_NARGS(__VA_ARGS__), __VA_ARGS__)


/// Shutdown the HPX runtime.
///
/// This causes the hpx_run() in the main native thread to return the @p code.
/// The returned thread is executing the original native thread, and all
/// supplementary scheduler threads and network will have been shutdown, and any
/// library resources will have been cleaned up.
///
/// This call is cooperative and synchronous, so it may not return if there are
/// misbehaving HPX lightweight threads.
///
/// @param code a status code to be returned by hpx_run()
void hpx_shutdown(int code)
  HPX_NORETURN;


/// Abort the HPX runtime.
///
/// This causes the main native thread to return the @p code from hpx_run(). The
/// returned thread is executing the original native thread, and all
/// supplementary scheduler threads an network will ahve been shutdown.
///
/// This is not cooperative, and will not clean up any resources. Furthermore,
/// the state of the system after the return is not well defined. The
/// application's main native thread should only rely on the async-safe
/// interface provided in signal(7).
void hpx_abort(void)
  HPX_NORETURN;


/// Print the help string associated with the runtime configuration
/// options supported by the HPX runtime.
///
void hpx_print_help(void);

#endif
