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
#ifndef HPX_RUNTIME_H
#define HPX_RUNTIME_H

#include "hpx/attributes.h"
#include "hpx/hpx_action.h"
#include "hpx/hpx_config.h"

/// ----------------------------------------------------------------------------
/// HPX system interface.
///
/// hpx_init() initializes the scheduler, network, and locality
///
/// hpx_register_action() register a user-level action with the runtime.
///
/// hpx_run() is called from the native thread after hpx_init() and action
/// registration is complete, in order
///
/// hpx_abort() is called from an HPX lightweight thread to terminate scheduler
/// execution asynchronously
///
/// hpx_shutdown() is called from an HPX lightweight thread to terminate
/// scheduler execution
/// ----------------------------------------------------------------------------
int hpx_init(const hpx_config_t *config);


/// ----------------------------------------------------------------------------
/// This finalizes action registration, starts up any scheduler and native
/// threads that need to run, and transfers all control into the HPX scheduler,
/// beginning execution in @p entry. Returns the hpx_shutdown() code.
///
/// The @p entry paramter may be HPX_ACTION_NULL, in which case this entire
/// scheduler instance is running, waiting for a successful inter-locality steal
/// operation (if that is implemented) or a network parcel.
/// ----------------------------------------------------------------------------
int hpx_run(hpx_action_t entry, const void *args, unsigned size);


/// ----------------------------------------------------------------------------
/// This causes the main native thread to return the @p code from hpx_run(). The
/// returned thread is executing the original native thread, and all
/// supplementary scheduler threads and network will have been shutdown, and any
/// library resources will have been cleaned up.
///
/// This call is cooperative and synchronous, so it may not return if there are
/// misbehaving HPX lightweight threads.
/// ----------------------------------------------------------------------------
void hpx_shutdown(int code) HPX_NORETURN;


/// ----------------------------------------------------------------------------
/// This causes the main native thread to return the @p code from hpx_run(). The
/// returned thread is executing the original native thread, and all
/// supplementary scheduler threads an network will ahve been shutdown.
///
/// This is not cooperative, and will not clean up any resources. Furthermore,
/// the state of the system after the return is not well defined. The
/// application's main native thread should only rely on the async-safe
/// interface provided in signal(7).
/// ----------------------------------------------------------------------------
void hpx_abort(void) HPX_NORETURN;


#endif
