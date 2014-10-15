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
#ifndef HPX_ADDR_H
#define HPX_ADDR_H

/// @file
/// @brief Types and functions specific to dealing with global addresses.

/// An HPX global address.
///
/// HPX manages global addresses on a per-block basis. Blocks are
/// allocated during hpx_gas_alloc() or hpx_gas_global_alloc(), and can be
/// up to 2^32 bytes large.
typedef uint64_t hpx_addr_t;


/// Compare two global addresses
///
/// @param lhs a global address
/// @param rhs a global address
/// @returns   true if the addresses are equal
bool hpx_addr_eq(const hpx_addr_t lhs, const hpx_addr_t rhs);


/// Perform global address arithmetic
///
/// Get the address of @p bytes into memory with address @p addr .
/// @param  addr a global address
/// @param bytes an offset in bytes into the memory referenced by @p addr
/// @returns     the address of the memory at offset @p bytes from @p addr
hpx_addr_t hpx_addr_add(const hpx_addr_t addr, int bytes, uint32_t block_size);

/// The equivalent of NULL for global memory
static const hpx_addr_t HPX_NULL = 0;

/// An address representing this locality in general;suitable for use as a
/// destination
extern hpx_addr_t HPX_HERE;

/// An address representing some other locality, suitable for use as a
/// destination.
/// @param i a locality
/// @returns an address representing that locality
hpx_addr_t HPX_THERE(uint32_t i);

#endif
