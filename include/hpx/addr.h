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

/// ----------------------------------------------------------------------------
/// An HPX global address.
///
/// HPX manages global addresses on a per-block basis. Blocks are allocated
/// during hpx_alloc or hpx_global_alloc, and can be up to 2^32 bytes large.
/// ----------------------------------------------------------------------------
typedef struct {
  uint64_t offset;                              // absolute offset
  uint32_t base_id;                             // base block id
  uint32_t block_bytes;                         // number of bytes per block
} hpx_addr_t;

#define HPX_ADDR_INIT(OFFSET, BASE, BYTES)       \
  {                                              \
    .offset = (OFFSET),                          \
    .base_id = (BASE),                           \
    .block_bytes = (BYTES)                       \
  }

/// ----------------------------------------------------------------------------
/// ----------------------------------------------------------------------------
hpx_addr_t hpx_addr_init(uint64_t offset, uint32_t base, uint32_t bytes);

/// ----------------------------------------------------------------------------
/// returns true if the addresses are equal
/// ----------------------------------------------------------------------------
bool hpx_addr_eq(const hpx_addr_t lhs, const hpx_addr_t rhs);


/// ----------------------------------------------------------------------------
/// global address arithmetic
/// ----------------------------------------------------------------------------
hpx_addr_t hpx_addr_add(const hpx_addr_t addr, int bytes);


extern const hpx_addr_t HPX_NULL;
extern hpx_addr_t HPX_HERE;
hpx_addr_t HPX_THERE(int i);


#endif
