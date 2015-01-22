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
#ifndef HPX_RPC_H
#define HPX_RPC_H

/// @file
/// @brief HPX remote procedure call interface

/// Fully synchronous call interface.
///
/// Performs @p action on @p args at @p addr, and sets @p out with the
/// resulting value. The output value @p out can be NULL (or the
/// corresponding @p olen could be zero), in which case no return
/// value is generated.
///
/// @param   addr The address that defines where the action is executed.
/// @param action The action to perform.
/// @param    out Address of the output buffer.
/// @param   olen The length of the @p output buffer.
/// @param   args The argument data buffer for @p action.
/// @param   alen The length of the @p args buffer.
///
/// @returns HPX_SUCCESS, or an error code if the action generated an error that
///          could not be handled remotely/
int hpx_call_sync(hpx_addr_t addr, hpx_action_t action, void *out, size_t olen, ...);


/// Locally synchronous call interface.
///
/// This is a locally-synchronous, globally-asynchronous variant of
/// the remote-procedure call interface. If @p result is not HPX_NULL,
/// hpx_call puts the the resulting value in @p result at some point
/// in the future.
///
/// @param   addr The address that defines where the action is executed.
/// @param action The action to perform.
/// @param result An address of an LCO to trigger with the result.
/// @param   args The argument data buffer for @p action.
/// @param    len The length of the @p args buffer.
///
/// @returns HPX_SUCCESS, or an error code if there was a problem locally during
///          the hpx_call invocation.
int hpx_call(hpx_addr_t addr, hpx_action_t action, hpx_addr_t result, ...);


/// Locally synchronous call with continuation interface.
///
/// This is similar to hpx_call with additional parameters to specify
/// the continuation action @p c_action to be executed at a
/// continuation address @p c_target.
///
/// @param   addr   The address that defines where the action is executed.
/// @param action   The action to perform.
/// @param c_target The address where the continuation action is executed.
/// @param c_action The continuation action to perform.
/// @param   args   The argument data buffer for @p action.
/// @param    len   The length of the @p args buffer.
///
/// @returns HPX_SUCCESS, or an error code if there was a problem locally during
///          the hpx_call invocation.
int hpx_call_with_continuation(hpx_addr_t addr, hpx_action_t action,
                               hpx_addr_t c_target, hpx_action_t c_action, ...);


/// Fully asynchronous call interface.
///
/// This is a completely asynchronous variant of the remote-procedure
/// call interface. If @p result is not HPX_NULL, hpx_call puts the
/// the resulting value in @p result at some point in the future. This
/// function returns even before the argument buffer has been copied
/// and is free to reuse. If @p lsync is not HPX_NULL, it is set
/// when @p args is safe to be reused or freed.
///
/// @param       addr The address that defines where the action is executed.
/// @param     action The action to perform.
/// @param      lsync The global address of an LCO to signal local completion
///                   (i.e., R/W access to, or free of @p args is safe),
///                   HPX_NULL if we don't care.
/// @param     result The global address of an LCO to signal with the result.
/// @param       args The argument data buffer for @p action.
/// @param        len The length of the @p args buffer.
///
/// @returns HPX_SUCCESS, or an error code if there was a problem locally during
///          the hpx_call_async invocation.
int hpx_call_async(hpx_addr_t addr, hpx_action_t action, hpx_addr_t lsync,
                   hpx_addr_t result, ...);


/// Call with current continuation.
///
/// This calls an action passing the currrent thread's continuation as
/// the continuation for the called action. It finishes the current
/// thread's execution, and does not yield control back to the thread.
///
/// @param    addr The address where the action is executed.
/// @param  action The action to perform.
/// @param cleanup A callback function that is run after the action
///                has been invoked.
/// @param     env The environment to pass to the cleanup function.
/// @param    args The argument data buffer for @p action.
/// @param     len The length of the @p args buffer.
///
/// @returns HPX_SUCCESS, or an error code if there was a problem during
///          the hpx_call_cc invocation.
int hpx_call_cc(hpx_addr_t addr, hpx_action_t action, void (*cleanup)(void*),
                void *env, ...);


/// HPX collective operations.
///
/// This is a parallel call interface that performs an @p action on @p args at
/// all available localities. The output values are not returned, but the
/// completion of the broadcast operation can be tracked through the @p lco LCO.
/// @param action the action to perform
/// @param    lco the address of an LCO to trigger when the broadcast operation
///               is completed
/// @param   args the argument data for @p action
/// @param    len the length of @p args
///
/// @returns      HPX_SUCCESS if no errors were encountered
int hpx_bcast(hpx_action_t action, hpx_addr_t lco, const void *args, size_t len);

/// HPX collective operations.
///
/// This is a parallel call interface that performs an @p action on @p args at
/// all available localities. The output values are not returned.
///
/// @param action the action to perform
/// @param   args the argument data for @p action
/// @param    len the length of @p args
///
/// @returns      HPX_SUCCESS if no errors were encountered
int hpx_bcast_sync(hpx_action_t action, const void *args, size_t len);

#endif
