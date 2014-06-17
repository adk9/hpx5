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

/// ----------------------------------------------------------------------------
/// HPX call interface (completely synchronous).
///
/// Performs @p action on @p args at @p addr, and sets @p out with the
/// resulting value. The output value @p out can be NULL (or the
/// corresponding @p olen could be zero), in which case no return
/// value is generated.
///
/// @param   addr - the address that defines where the action is executed
/// @param action - the action to perform
/// @param   args - the argument data for @p action
/// @param   alen - the length of @p args
/// @param    out - address of  the result
/// @param   olen - the length of the @p output
/// @returns      - HPX_SUCCESS, or an error code if the action generated an
///                 error that could not be handled remotely
/// ----------------------------------------------------------------------------
int hpx_call_sync(hpx_addr_t addr, hpx_action_t action, const void *args,
                  size_t alen, void *out, size_t olen);


/// ----------------------------------------------------------------------------
/// HPX call interface.
///
/// This is a locally-synchronous, globally-asynchronous variant of
/// the remote-procedure call interface. If @p result is not HPX_NULL,
/// hpx_call puts the the resulting value in @p result at some point
/// in the future.
///
/// @param   addr - the address that defines where the action is executed
/// @param action - the action to perform
/// @param   args - the argument data for @p action
/// @param    len - the length of @p args
/// @param result - an address of an LCO to trigger with the result
/// ----------------------------------------------------------------------------
int hpx_call(hpx_addr_t addr, hpx_action_t action, const void *args,
             size_t len, hpx_addr_t result);


/// ----------------------------------------------------------------------------
/// HPX call interface (completely asynchronous).
///
/// This is a completely asynchronous variant of the remote-procedure
/// call interface. If @p result is not HPX_NULL, hpx_call puts the
/// the resulting value in @p result at some point in the future. This
/// function returns even before the argument buffer has been copied
/// and is free to reuse. If @p args_reuse is not HPX_NULL, it is set
/// when @p args is safe to be reused or freed.
///
/// @param   addr     - the address that defines where the action is executed
/// @param action     - the action to perform
/// @param   args     - the argument data for @p action
/// @param    len     - the length of @p args
/// @param args_reuse - an address of an LCO to trigger when the
///                     argument buffer is safe to be reused (local completion)
/// @param result     - an address of an LCO to trigger with the result
/// ----------------------------------------------------------------------------
int hpx_call_async(hpx_addr_t addr, hpx_action_t action, const void *args,
                   size_t len, hpx_addr_t args_reuse, hpx_addr_t result);


/// ----------------------------------------------------------------------------
/// HPX collective operations.
///
/// This is a parallel call interface that performs an @p action on @p args at
/// all available localities. The output values are not returned, but the
/// completion of the broadcast operation can be tracked through the @p lco LCO.
/// ----------------------------------------------------------------------------
int hpx_bcast(hpx_action_t action, const void *args, size_t len,
              hpx_addr_t lco);

#endif
