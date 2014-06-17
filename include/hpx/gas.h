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
#ifndef HPX_GAS_H
#define HPX_GAS_H

#include "hpx/addr.h"

/// ----------------------------------------------------------------------------
/// Performs address translation.
///
/// This will try and perform a global-to-local translation on the global @p
/// addr, and set @p out to the local address if it is successful. If @p local
/// is NULL, then this only performs address translation. If the address is not
/// local, or @p is not NULL and the pin fails, this will return false,
/// otherwise it will return true.
///
/// Successful pinning operations must be matched with an unpin operation, if
/// the underlying data is ever to be moved.
///
/// @param       addr - the global address
/// @param[out] local - the pinned local address
/// @returns          - { true; if @p addr is local and @p local is NULL
///                       true; if @p addr is local and @p is not NULL and pin
///                             is successful
///                       false; if @p is not local
///                       false; if @p is local and @local is not NULL and pin
///                              fails
/// ----------------------------------------------------------------------------
bool hpx_gas_try_pin(const hpx_addr_t addr, void **local);


/// ----------------------------------------------------------------------------
/// Allows the address to be remapped.
/// ----------------------------------------------------------------------------
void hpx_gas_unpin(const hpx_addr_t addr);


/// ----------------------------------------------------------------------------
/// Allocate distributed global memory.
///
/// This is not a collective operation, the returned address is returned only to
/// the calling thread, and must either be written into already-allocated global
/// memory, or sent via a parcel, for anyone else to address the allocation.
///
/// In UPC-land, the returned global address would have the following
/// distribution.
///
/// shared [bytes] char foo[n * bytes];
/// ----------------------------------------------------------------------------
hpx_addr_t hpx_gas_global_alloc(size_t n, uint32_t bytes);


/// ----------------------------------------------------------------------------
/// Allocate local global memory.
///
/// The returned address is local to the calling locality, and not distributed,
/// but can be used from any locality.
/// ----------------------------------------------------------------------------
hpx_addr_t hpx_gas_alloc(size_t n, uint32_t bytes);


/// ----------------------------------------------------------------------------
/// Free a global allocation.
/// ----------------------------------------------------------------------------
void hpx_gas_global_free(hpx_addr_t addr);


/// ----------------------------------------------------------------------------
/// Change the locality-affinity of a global distributed memory address.
///
/// This operation is only valid in the AGAS GAS mode. For PGAS, it is effectively
/// a no-op.
///
/// @param         src - the source address to move
/// @param         dst - the address pointing to the target locality to move the
///                      source address @p src to move to.
/// @param[out]    lco - LCO object to check the completion of move.
/// ----------------------------------------------------------------------------
void hpx_gas_move(hpx_addr_t src, hpx_addr_t dst, hpx_addr_t lco);


#endif
