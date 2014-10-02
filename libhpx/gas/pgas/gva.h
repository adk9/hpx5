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
#ifndef LIBHPX_GAS_PGAS_ADDR_H
#define LIBHPX_GAS_PGAS_ADDR_H

/// @file libhpx/gas/pgas/addr.h
/// @brief Declaration of the PGAS-specific address.
#include <stdint.h>
#include <hpx/hpx.h>

hpx_locality_t pgas_locality_of(hpx_addr_t addr)
  HPX_INTERNAL;

uint64_t pgas_offset_of(hpx_addr_t gva)
  HPX_INTERNAL;

uint16_t pgas_phase_of(hpx_addr_t gva)
  HPX_INTERNAL;

/// Compute the (signed) distance, in bytes, between two global addresses.
///
/// This is only valid if @p lhs and @p rhs come from the same global
/// allocation.
///
/// @param   lhs The left-hand-side address.
/// @param   rhs The right-hand-size address.
/// @param bsize The block size for the allocation.
///
/// @returns The equivalent of (@p lhs - @p rhs) if @p lhs and @p rhs were
///          char*.
int64_t pgas_sub(hpx_addr_t lhs, hpx_addr_t rhs, uint16_t bsize)
  HPX_INTERNAL;

hpx_addr_t pgas_add(hpx_addr_t gva, int64_t bytes, uint16_t bsize)
  HPX_INTERNAL;

#endif // LIBHPX_GAS_PGAS_ADDR_H
