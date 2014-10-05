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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


/// @file libhpx/gas/addr.c
/// AGAS agnostic global address manipulation.
///
/// Allocation is done on a per-block basis.
///
/// There are 2^32 blocks available, and the maximum size for a block is 2^32
/// bytes. Most blocks are smaller than this. Each block is backed by at least 1
/// page allocation. There are some reserved blocks.
///
/// In particular, each rank has a reserved block to deal with its local
/// allocations, i.e., block 0 belongs to rank 0, block 1 belongs to rank 1,
/// etc. Rank 0's block 0 also serves as the "NULL" block, that contains
/// HPX_NULL.

#include "hpx/hpx.h"

/// Null doubles as rank 0's HPX_HERE.
// const hpx_addr_t HPX_NULL = HPX_ADDR_INIT(0, 0, 0);


/// Updated in hpx_init(), the HPX_HERE (HPX_THERE) block is a max-bytes
/// block. This means that any reasonable address computation within a here or
/// there address will remain on the same locality.
// hpx_addr_t HPX_HERE = HPX_ADDR_INIT(0, 0, UINT32_MAX);


/// Uses the well-known, low-order, block mappings to construct a "there
/// address."
// hpx_addr_t HPX_THERE(hpx_locality_t i) {
//   assert(i != HPX_LOCALITY_ALL);
//   if (i == HPX_LOCALITY_NONE)
//     return HPX_NULL;
//   hpx_addr_t addr = hpx_addr_init(0, i, UINT32_MAX);
//   return addr;
// }


// bool hpx_addr_eq(const hpx_addr_t lhs, const hpx_addr_t rhs) {
//   return (lhs.offset == rhs.offset) && (lhs.base_id == rhs.base_id) &&
//   (lhs.block_bytes == rhs.block_bytes);
// }


// /// Perform address arithmetic.
// hpx_addr_t hpx_addr_add(const hpx_addr_t addr, int bytes) {
//   // not checking overflow
//   uint64_t offset = addr.offset + bytes;
//   hpx_addr_t result = hpx_addr_init(offset, addr.base_id, addr.block_bytes);
//   return result;
// }
