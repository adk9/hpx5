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
#ifndef LIBHPX_SCHEDULER_ENTRY_H
#define LIBHPX_SCHEDULER_ENTRY_H

#include "hpx.h"

/// ----------------------------------------------------------------------------
/// All user-level (HPX) threads start in this entry function.
///
/// This function extracts the handler for the parcel's action, and executes it
/// on the parcel's arguments. Actions may terminate in one of two ways, the
/// action may return an error code (possibly HPX_SUCCESS) if it doesn't return
/// a value, or it may call hpx_thread_exit() with an error code and a return
/// value.
///
/// If the action returns to us, we need to deal with the error condition, or,
/// if it returns HPX_SUCCESS, then we redirect to hpx_exit_thread() to
/// terminate execution.
///
/// @param parcel - the parcel that describes this thread's task
/// ----------------------------------------------------------------------------
HPX_INTERNAL void scheduler_thread_entry(hpx_parcel_t *parcel) HPX_NORETURN;

/// ----------------------------------------------------------------------------
/// This is a callback that can be used to clean up resources from a canceled
/// scheduler thread.
/// ----------------------------------------------------------------------------
HPX_INTERNAL void scheduler_thread_shutdown(void);

#endif // LIBHPX_SCHEDULER_ENTRY_H
