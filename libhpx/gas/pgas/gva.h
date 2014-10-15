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
uint64_t pgas_gva_heap_offset_of(hpx_addr_t gva, uint32_t ranks)
  HPX_INTERNAL;

/// Create a global virtual address from a locality and heap offset pair.
///
/// We need to know the number of ranks to perform this operation because that
/// will tell us how many bits we are using to describe the rank.
///
/// @param      locality The locality where we want the address to point.
/// @param   heap_offset The offset into the heap.
/// @param         ranks The number of ranks overall in the system
///
/// @returns A global address that contains the appropriate triple of
///          information.
hpx_addr_t pgas_gva_from_heap_offset(uint32_t locality, uint64_t heap_offset,
                                     uint32_t ranks)
  HPX_INTERNAL;

/// Create a global virtual address from a locality, gva-offset, and phase.
///
/// We need to know both the number of ranks and the block size in order to
/// determine how many bits in the address we're using to store the locality and
/// the phase.
///
/// @param      locality The locality where we want the address to point.
/// @param        offset The offset chunk of the address.
/// @param         phase The phase we want for the address
/// @param         ranks The number of ranks overall in the system
/// @param         bsize The block size for the allocation
///
/// @returns The encoded gva.
hpx_addr_t pgas_gva_from_triple(uint32_t locality, uint64_t offset,
                                uint32_t phase, uint32_t ranks, uint32_t bsize)
  HPX_INTERNAL;

/// Compute the (signed) distance, in bytes, between two global addresses.
///
/// This is only valid if @p lhs and @p rhs come from the same global
/// allocation.
///
/// @param   lhs The left-hand-side address.
/// @param   rhs The right-hand-size address.
/// @param ranks The number of localities in the system.
/// @param bsize The block size for the allocation.
///
/// @returns The equivalent of (@p lhs - @p rhs) if @p lhs and @p rhs were
///          char*.
int64_t pgas_gva_sub(hpx_addr_t lhs, hpx_addr_t rhs, uint32_t ranks,
                     uint32_t bsize)
  HPX_INTERNAL;

/// Compute cyclic address arithmetic on the global address.
///
/// @param ranks The number of localities in the system.
/// @param bsize The block size in bytes for the allocation.

hpx_addr_t pgas_gva_add_cyclic(hpx_addr_t gva, int64_t bytes, uint32_t ranks,
                               uint32_t bsize)
  HPX_INTERNAL;


/// Compute standard address arithmetic on the global address.
hpx_addr_t pgas_gva_add(hpx_addr_t gva, int64_t bytes, uint32_t ranks)
  HPX_INTERNAL;


#endif // LIBHPX_GAS_PGAS_GVA_H
