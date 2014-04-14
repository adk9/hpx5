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
#ifndef LIBHPX_GAS_ADDR_H
#define LIBHPX_GAS_ADDR_H

#include "hpx/hpx.h"

static inline uint32_t addr_block_id(hpx_addr_t addr) {
  assert(addr.block_bytes);
  return addr.base_id + (addr.offset / addr.block_bytes);
}

static inline void *addr_to_local(hpx_addr_t addr, void *base) {
  return (void*)((char*)base + addr.offset % addr.block_bytes);
}

#endif // LIBHPX_GAS_ADDR_H
