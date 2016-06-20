// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2016, Trustees of Indiana University,
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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libhpx/locality.h>
#include <libhpx/memory.h>
#include "agas.h"
#include "btt.h"
#include "gva.h"

static int
_locality_alloc_cyclic_handler(uint64_t blocks, uint32_t align, uint64_t offset,
                               void *lva, uint32_t attr, int zero) {
  agas_t *agas = (agas_t*)here->gas;
  size_t bsize = 1u << align;
  if (here->rank != 0) {
    size_t boundary = (bsize < 8) ? 8 : bsize;
    lva = NULL;
    int e = posix_memalign(&lva, boundary, blocks * bsize);
    dbg_check(e, "Failed memalign\n");
    (void)e;
  }

  if (zero) {
    lva = memset(lva, 0, blocks * bsize);
  }

  // and insert entries into our block translation table
  gva_t gva = {
    .bits = {
      .offset = offset,
      .cyclic = 1,
      .size = align,
      .home = here->rank
    }
  };

  for (int i = 0; i < blocks; i++) {
    btt_insert(agas->btt, gva, here->rank, lva, blocks, attr);
    lva += bsize;
    gva.bits.offset += bsize;
  }
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, 0, _locality_alloc_cyclic,
                     _locality_alloc_cyclic_handler, HPX_UINT64, HPX_UINT32,
                     HPX_UINT64, HPX_POINTER, HPX_UINT32, HPX_INT);

static hpx_addr_t
_agas_alloc_cyclic_handler(size_t n, size_t bbsize, uint32_t attr, int zero) {
  agas_t *agas = (agas_t*)here->gas;
  dbg_assert(here->rank == 0);
  dbg_assert(bbsize <= agas->vtable.max_block_size);
  uint32_t bsize = bbsize;

  // Figure out how many blocks per node we need.
  uint64_t blocks = ceil_div_64(n, here->ranks);
  uint32_t  align = ceil_log2_32(bsize);
  dbg_assert(align <= 32);
  size_t padded = 1u << align;

  agas_alloc_bsize = padded;
  // Allocate the blocks as a contiguous, aligned array from cyclic memory.
  void *lva = cyclic_memalign(padded, blocks * padded);
  if (!lva) {
    dbg_error("failed cyclic allocation\n");
  }

  gva_t gva = agas_lva_to_gva(agas, lva, padded);
  gva.bits.cyclic = 1;
  uint64_t offset = gva.bits.offset;
  int e = hpx_bcast_rsync(_locality_alloc_cyclic, &blocks, &align, &offset,
                          &lva, &attr, &zero);
  dbg_check(e, "failed to insert btt entries.\n");
  return gva.addr;
}
static LIBHPX_ACTION(HPX_DEFAULT, 0, _agas_alloc_cyclic,
                     _agas_alloc_cyclic_handler, HPX_SIZE_T, HPX_SIZE_T,
                     HPX_UINT32, HPX_INT);

hpx_addr_t agas_alloc_cyclic(size_t n, size_t bbsize, uint32_t boundary,
                             uint32_t attr) {
  uint32_t bsize = bbsize;

  int zero = 0;
  hpx_addr_t addr;
  int e = hpx_call_sync(HPX_THERE(0), _agas_alloc_cyclic, &addr, sizeof(addr),
                        &n, &bsize, &attr, &zero);
  dbg_check(e, "Failed to call agas_alloc_cyclic.\n");
  dbg_assert_str(addr != HPX_NULL, "HPX_NULL is not a valid allocation\n");
  return addr;
}

hpx_addr_t agas_calloc_cyclic(size_t n, size_t bbsize, uint32_t boundary,
                              uint32_t attr) {
  uint32_t bsize = bbsize;

  int zero = 1;
  hpx_addr_t addr;
  int e = hpx_call_sync(HPX_THERE(0), _agas_alloc_cyclic, &addr, sizeof(addr),
                        &n, &bsize, &attr, &zero);
  dbg_check(e, "Failed to call agas_calloc_cyclic.\n");
  dbg_assert_str(addr != HPX_NULL, "HPX_NULL is not a valid allocation\n");
  return addr;
}
