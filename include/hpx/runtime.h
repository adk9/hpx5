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

#ifndef HPX_RUNTIME_H
#define HPX_RUNTIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <hpx/attributes.h>
#include <hpx/builtins.h>
#include <hpx/action.h>

/// @defgroup system System
/// @file include/hpx/runtime.h
/// @brief Functions that control the overall runtime
/// @{

/// Initializes the HPX runtime.
///
/// This must be called before other HPX functions.  hpx_init() initializes the
/// scheduler, network, and locality and should be called before any other HPX
/// functions.
///
/// @param         argc Count of command-line arguments.
/// @param         argv Array of command-line arguments.
/// @returns      HPX_SUCCESS on success
int hpx_init(int *argc, char ***argv)
  HPX_PUBLIC;

/// Finalize/cleanup from the HPX runtime.
///
/// This function will remove almost all data structures and allocations, and
/// will finalize the underlying network implementation. Note that hpx_run()
/// must never be called after hpx_finalize().
void hpx_finalize()
  HPX_PUBLIC;

/// Start an HPX main process.
///
/// This collective creates an HPX "main" process, and calls the given action @p
/// entry in the context of this process.
///
/// * The @p entry action is invoked only on the root locality and represents a
///   diffusing computation.
/// * The process does not use termination detection and must be terminated
///   through a single explicit call to hpx_exit().
/// * On termination, it deletes the main process and returns the status
///   returned by hpx_exit().
///
/// @param        entry An action to execute.
/// @param[out]     out A user buffer for output (may be NULL).
/// @param        nargs The number of arguments to pass to @p entry.
/// @param          ... Pointers to the arguments for @p entry.
///
/// @returns            The status code passed to hpx_exit() upon termination.
int _hpx_run(hpx_action_t *entry, void *out, int nargs, ...)
  HPX_PUBLIC;

#define hpx_run(entry, out, ...)                                    \
  _hpx_run(entry, out, __HPX_NARGS(__VA_ARGS__) , ##__VA_ARGS__)

/// Start an HPX main process.
///
/// This creates an HPX "main" process, and calls the given action @p entry in
/// the context of this process.
///
/// * The @p entry action is invoked at startup on every locality.
/// * The process will terminate implicitly when all of the @p entry threads
///   terminate completely. Outstanding asynchronous operations may result in
///   undefined behavior.
/// * On termination the main process is deleted and the value returned will be
///   zero for success and non-zero for failure.
///
/// @param        entry An action to execute.
/// @param[out]     out A user buffer for output (may be NULL).
/// @param        nargs The number of arguments to pass to @p entry.
/// @param          ... Pointers to the arguments for @p entry.
///
/// @returns            Zero for success and non-zero for failure.
int _hpx_run_spmd(hpx_action_t *entry, void *out, int nargs, ...)
  HPX_PUBLIC;

#define hpx_run_spmd(entry, out, ...)                                   \
  _hpx_run_spmd(entry, out, __HPX_NARGS(__VA_ARGS__) , ##__VA_ARGS__)

/// Exit the HPX runtime.
///
/// This causes the hpx_run() in the main native thread to return the @p
/// code. If an @p out value is provided it will be broadcast to all
/// participants in the hpx_run() collective. It is safe to call hpx_run() again
/// after hpx_exit().
///
/// This call does not imply that the HPX runtime has shut down. In particular,
/// system threads may continue to run and execute HPX high-speed network
/// progress or outstanding lightweight threads. Users should ensure that such
/// concurrent activity will not create detrimental data races in their
/// applications.
///
/// @note This routine enacts non-local control flow, however the runtime must
///       correctly unwind the stack for languages that require such behavior
///       (e.g., C++).
///
/// @note While this routine does not guarantee to suspend the runtime,
///       high-performance implementations are expected to reduce their resource
///       consumption as a result of this call. In particular, runtime-spawned
///       system threads should be suspended.
///
/// @param         code A status code to be returned by hpx_run().
/// @param        bytes The size of the data to be returned (may be 0).
/// @param          out A (possibly NULL) pointer to the data to be returned.
void hpx_exit(int code, size_t bytes, const void *out)
  HPX_PUBLIC HPX_NORETURN;

/// Abort the HPX runtime.
///
/// This is not cooperative, and will not clean up any resources.
void hpx_abort(void)
  HPX_PUBLIC HPX_NORETURN;

/// Print the help string associated with the runtime configuration
/// options supported by the HPX runtime.
void hpx_print_help(void)
  HPX_PUBLIC;

/// Print the version string associated with the HPX interface implemented by
/// the runtime.
void hpx_print_version(void)
  HPX_PUBLIC;

/// @}

#ifdef __cplusplus
}
#endif

#endif
