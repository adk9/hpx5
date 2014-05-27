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
/// HPX call interface.
///
/// Performs @p action on @p args at @p addr, and sets @p result with the
/// resulting value. HPX_NULL is value for @p result, in which case no return
/// value is generated.
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
/// HPX collective operations.
///
/// This is a parallel call interface that performs an @p action on @p args at
/// all available localities. The output values are not returned, but the
/// completion of the broadcast operation can be tracked through the @p lco LCO.
/// ----------------------------------------------------------------------------
int hpx_bcast(hpx_action_t action, const void *args, size_t len,
              hpx_addr_t lco);

#endif
