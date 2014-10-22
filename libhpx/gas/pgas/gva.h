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
#ifndef LIBHPX_GAS_PGAS_GVA_H
#define LIBHPX_GAS_PGAS_GVA_H

/// @file libhpx/gas/pgas/addr.h
/// @brief Declaration of the PGAS-specific address.
#include <stdint.h>
#include <hpx/hpx.h>

/// Extract the locality from a gva.
uint32_t pgas_gva_to_rank(hpx_addr_t gva)
  HPX_INTERNAL;


/// Extract the heap offset of a gva, given the number of ranks.
///
/// The heap_offset is the complete relative offset of the global virtual
/// address in its global heap. It encodes both the gva offset and the gva
/// phase, and can be used to extract either of those in the future.
///
/// @param      gva The global address.
///
/// @returns The offset within the global heap that corresponds to the address.
uint64_t pgas_gva_to_offset(hpx_addr_t gva)
  HPX_INTERNAL;


/// Create a global virtual address from a locality and heap offset pair.
///
/// @param locality The locality where we want the address to point.
/// @param   offset The offset into the heap.
///
/// @returns A global address representing the offset at the locality.
hpx_addr_t pgas_offset_to_gva(uint32_t locality, uint64_t offset)
  HPX_INTERNAL;


/// Compute the (signed) distance, in bytes, between two global addresses from a
/// cyclic allocation.
///
/// This is only valid if @p lhs and @p rhs come from the same global
/// allocation.
///
/// @param      lhs The left-hand-side address.
/// @param      rhs The right-hand-size address.
/// @param    bsize The block size for the allocation.
///
/// @returns The difference between the two addresses such that
///          @code
///          lhs == pgas_gva_add_cyclic(rhs,
///                                     pgas_gva_sub_cyclic(lhs, rhs, bsize),
///                                     bsize)
///          @endcode
int64_t pgas_gva_sub_cyclic(hpx_addr_t lhs, hpx_addr_t rhs, uint32_t bsize)
  HPX_INTERNAL;


/// Compute the (signed) gistance, in bytes, between two global addresses.
///
/// This is only balid if @p lhs and @p rhs come from the same global
/// allocation.
///
/// @param      lhs The left-hand-side address.
/// @param      rhs The right-hand-side address.
///
/// @returns lhs - rhs
int64_t pgas_gva_sub(hpx_addr_t lhs, hpx_addr_t rhs)
  HPX_INTERNAL;


/// Compute cyclic address arithmetic on the global address.
///
/// @param      gva The global address base.
/// @param    bytes The displacement in the number of bytes (may be negative).
/// @param    bsize The block size in bytes for the allocation.
///
/// @returns The global address representation that is @p bytes away from
///          @gva.
hpx_addr_t pgas_gva_add_cyclic(hpx_addr_t gva, int64_t bytes, uint32_t bsize)
  HPX_INTERNAL;


/// Compute standard address arithmetic on the global address.
hpx_addr_t pgas_gva_add(hpx_addr_t gva, int64_t bytes)
  HPX_INTERNAL;


#endif // LIBHPX_GAS_PGAS_GVA_H
