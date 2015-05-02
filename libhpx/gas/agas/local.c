// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <libhpx/memory.h>
#include "agas.h"
#include "btt.h"
#include "gva.h"

void agas_local_free(agas_t *agas, gva_t gva, void *lva, hpx_addr_t rsync) {
  // how many blocks are involved in this mapping?
  size_t blocks = btt_get_blocks(agas->btt, gva);
  uint32_t bsize = 1 << gva.bits.size;

  // 1) remove this mapping
  for (int i = 0; i < blocks; ++i) {
    btt_remove(agas->btt, gva);
    gva.bits.offset += bsize;
  }

  // 2) free the backing memory---it didn't move because that isn't supported
  global_free(lva);

  // 3) set the lsync
  hpx_lco_error(rsync, HPX_SUCCESS, HPX_NULL);
}


int64_t
agas_local_sub(const agas_t *agas, gva_t lhs, gva_t rhs, uint32_t bsize) {
  uint64_t bits = lhs.bits.size;
  uint64_t mask = (1lu << bits) - 1;
  uint64_t plhs = lhs.bits.offset & mask;
  uint64_t prhs = rhs.bits.offset & mask;
  uint64_t blhs = lhs.bits.offset >> bits;
  uint64_t brhs = rhs.bits.offset >> bits;
  return (plhs - prhs) + (blhs - brhs) * bsize;
}

hpx_addr_t
agas_local_add(const agas_t *agas, gva_t gva, int64_t n, uint32_t bsize) {
  int64_t blocks = n / bsize;
  int64_t bytes = n % bsize;
  uint64_t block_size = (1lu << gva.bits.size);
  uint64_t addr = gva.addr + blocks * block_size + bytes;
  dbg_assert((addr & (block_size - 1)) < bsize);
  return addr;
}
