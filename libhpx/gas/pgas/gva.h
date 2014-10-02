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
#include <hpx/builtins.h>
#include "libhpx/debug.h"

typedef uint64_t pgas_gva_t;

/// From stack overflow.
///
/// http://stackoverflow.com/questions/3272424/compute-fast-log-base-2-ceiling
static inline uint32_t ceil_log2_32(uint32_t val){
  assert(val);
  return ((sizeof(val) * 8 - 1) - clz(val)) + (!!(val & (val - 1)));
}

/// Extract the locality from a gva, given the number of ranks.
static inline uint32_t pgas_gva_locality_of(pgas_gva_t gva, uint32_t ranks) {
  const uint32_t rshift = (sizeof(pgas_gva_t) * 8) - ceil_log2_32(ranks);
  return (uint32_t)(gva >> rshift);
}

static inline uint64_t pgas_gva_offset_of(pgas_gva_t gva, uint32_t ranks,
                                          uint32_t bsize) {
  const uint32_t lshift = ceil_log2_32(ranks);
  const uint32_t rshift = ceil_log2_32(bsize) + lshift;
  return (gva << lshift) >> rshift;
}

static inline uint64_t pgas_gva_goffset_of(pgas_gva_t gva, uint32_t ranks) {
  const uint32_t shift = ceil_log2_32(ranks);
  return (gva << shift) >> shift;
}

static inline uint32_t pgas_gva_phase_of(pgas_gva_t gva, uint32_t bsize) {
  const uint64_t mask = ceil_log2_32(bsize) - 1;
  return (uint32_t)(gva & mask);
}

static inline pgas_gva_t pgas_gva_from_goffset(uint32_t locality, uint64_t offset,
                                               uint32_t ranks) {
  DEBUG_IF (locality >= (1 << ceil_log2_32(ranks))) {
    dbg_error("locality %u must be less than %u\n", locality,
              1 << ceil_log2_32(ranks));
  }

  DEBUG_IF (pgas_gva_locality_of(offset, ranks) != 0) {
    dbg_error("global offset too large.\n");
  }

  const uint32_t shift = (sizeof(pgas_gva_t) * 8) - ceil_log2_32(ranks);
  return ((uint64_t)locality << shift) + offset;
}

static inline pgas_gva_t pgas_gva_from(uint32_t locality, uint64_t offset,
                                       uint32_t phase, uint32_t ranks,
                                       uint32_t bsize) {
  DEBUG_IF (phase >= (1 << ceil_log2_32(bsize))) {
    dbg_error("phase %u must be less than %u", phase, 1 << ceil_log2_32(bsize));
  }

  const uint32_t shift = ceil_log2_32(bsize);
  return pgas_gva_from_goffset(locality, (offset << shift) + phase, ranks);
}


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
int64_t pgas_gva_sub(pgas_gva_t lhs, pgas_gva_t rhs, uint32_t ranks,
                     uint32_t bsize)
  HPX_INTERNAL;

/// Compute standard address arithmetic on the global address.
///
/// @param ranks The number of localities in the system.
/// @param bsize The block size in bytes for the allocation.

pgas_gva_t pgas_gva_add(pgas_gva_t gva, int64_t bytes, uint32_t ranks,
                        uint32_t bsize)
  HPX_INTERNAL;

#endif // LIBHPX_GAS_PGAS_ADDR_H
