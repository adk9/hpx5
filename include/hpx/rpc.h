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
#ifndef HPX_RPC_H
#define HPX_RPC_H

#include "hpx/builtins.h"

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
///          could not be handled remotely.
int     _hpx_call_sync(hpx_addr_t addr, hpx_action_t action, void *out, size_t olen,
                       int nargs, ...);
#define hpx_call_sync(addr, action, out, olen, ...) \
  _hpx_call_sync(addr, action, out, olen, __HPX_NARGS(__VA_ARGS__), __VA_ARGS__)


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
int    _hpx_call(hpx_addr_t addr, hpx_action_t action, hpx_addr_t result,
                 int nargs, ...);
#define hpx_call(addr, action, result, ...) \
  _hpx_call(addr, action, result, __HPX_NARGS(__VA_ARGS__), __VA_ARGS__)


/// An experimental version of call that takes parameter symbols directly.
#define _HPX_ADDRESSOF(x) &x
#define hpx_xcall(addr, action, result, ...) \
  hpx_call(addr, action, result, __HPX_FOREACH(_HPX_ADDRESSOF, __VA_ARGS__))


/// Locally synchronous call interface when LCO is set.
///
/// This is a locally-synchronous, globally-asynchronous variant of
/// the remote-procedure call interface which implements the hpx_parcel_send_
/// through() function.
///
/// @param   gate The LCO that will serve as the gate.
/// @param   addr The address that defines where the action is executed.
/// @param action The action to perform.
/// @param result An address of an LCO to trigger with the result.
/// @param   args The argument data buffer for @p action.
/// @param    len The length of the @p args buffer.
///
/// @returns HPX_SUCCESS, or an error code if there was a problem locally during
///          the hpx_call invocation.
int    _hpx_call_when(hpx_addr_t gate, hpx_addr_t addr, hpx_action_t action,
                      hpx_addr_t result, int nargs, ...);
#define hpx_call_when(gate, addr, action, result, ...) \
  _hpx_call_when(gate, addr, action, result, __HPX_NARGS(__VA_ARGS__), __VA_ARGS__)

/// Locally synchronous call_when with continuation interface.
///
/// @param   gate   The LCO that will serve as the gate.
/// @param   addr   The address that defines where the action is executed.
/// @param action   The action to perform.
/// @param c_target The address where the continuation action is executed.
/// @param c_action The continuation action to perform.
/// @param   args   The argument data buffer for @p action.
/// @param    len   The length of the @p args buffer.
///
/// @returns HPX_SUCCESS, or an error code if there was a problem locally during
///          the hpx_call invocation.
int    _hpx_call_when_with_continuation(hpx_addr_t gate, hpx_addr_t addr,
                                        hpx_action_t action, hpx_addr_t c_target,
                                        hpx_action_t c_action, int nargs, ...);
#define hpx_call_when_with_continuation(gate, addr, action, c_target, c_action, ...) \
  _hpx_call_when_with_continuation(gate, addr, action, c_target, c_action,           \
                                   __HPX_NARGS(__VA_ARGS__), __VA_ARGS__)

/// Fully synchronous call interface which implements hpx_parcel_send_through()
/// when LCO is set
///
/// Performs @p action on @p args at @p addr, and sets @p out with the
/// resulting value. The output value @p out can be NULL (or the
/// corresponding @p olen could be zero), in which case no return
/// value is generated.
///
/// @param   gate The LCO that will serve as the gate.
/// @param   addr The address that defines where the action is executed.
/// @param action The action to perform.
/// @param    out Address of the output buffer.
/// @param   olen The length of the @p output buffer.
/// @param   args The argument data buffer for @p action.
/// @param   alen The length of the @p args buffer.
///
/// @returns HPX_SUCCESS, or an error code if the action generated an error that
///          could not be handled remotely.
int    _hpx_call_when_sync(hpx_addr_t gate, hpx_addr_t addr, hpx_action_t action,
                           void *out, size_t olen, int nargs, ...);
#define hpx_call_when_sync(gate, addr, action, out, olen, ...)          \
  _hpx_call_when_sync(gate, addr, action, out, olen,                    \
                      __HPX_NARGS(__VA_ARGS__), __VA_ARGS__)

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
int    _hpx_call_with_continuation(hpx_addr_t addr, hpx_action_t action,
                                   hpx_addr_t c_target, hpx_action_t c_action,
                                   int nargs, ...);
#define hpx_call_with_continuation(addr, action, c_target, c_action, ...) \
  _hpx_call_with_continuation(addr, action, c_target, c_action,           \
                              __HPX_NARGS(__VA_ARGS__), __VA_ARGS__)


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
int    _hpx_call_async(hpx_addr_t addr, hpx_action_t action, hpx_addr_t lsync,
                       hpx_addr_t result, int nargs, ...);
#define hpx_call_async(addr, action, lsync, result, ...)                \
  _hpx_call_async(addr, action, lsync, result, __HPX_NARGS(__VA_ARGS__),\
                  __VA_ARGS__)


/// Call with current continuation.
///
/// This calls an action passing the currrent thread's continuation as
/// the continuation for the called action. It finishes the current
/// thread's execution, and does not yield control back to the thread.
///
/// @param    gate An LCO for a dependent call.
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
void _hpx_call_when_cc(hpx_addr_t gate, hpx_addr_t addr, hpx_action_t action,
                       void (*cleanup)(void*), void *env, int nargs, ...)
  HPX_NORETURN;

#define hpx_call_when_cc(gate, addr, action, cleanup, env, ...) \
  _hpx_call_when_cc(gate, addr, action, cleanup, env,           \
                    __HPX_NARGS(__VA_ARGS__),##__VA_ARGS__)

#define hpx_call_cc(addr, action, cleanup, env, ...)                 \
  _hpx_call_when_cc(HPX_NULL, addr, action, cleanup, env,            \
                    __HPX_NARGS(__VA_ARGS__),##__VA_ARGS__)


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
/// @returns      HPX_SUCCESS if no errors were encountered.
int    _hpx_bcast(hpx_action_t action, hpx_addr_t lco, int nargs, ...);
#define hpx_bcast(action, lco, ...) \
  _hpx_bcast(action, lco, __HPX_NARGS(__VA_ARGS__), __VA_ARGS__)

/// HPX collective operations.
///
/// This is a parallel call interface that performs an @p action on @p args at
/// all available localities. The output values are not returned.
///
/// @param action the action to perform
/// @param   args the argument data for @p action
/// @param    len the length of @p args
///
/// @returns      HPX_SUCCESS if no errors were encountered.
int    _hpx_bcast_sync(hpx_action_t action, int nargs, ...);
#define hpx_bcast_sync(action, ...) \
  _hpx_bcast_sync(action, __HPX_NARGS(__VA_ARGS__), __VA_ARGS__)

#endif
