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

#include <stdio.h>
#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libhpx/memory.h>
#include "agas.h"
#include "btt.h"
#include "gva.h"

// The latter half of the free operation.
//
// This handler is called once for each block in the allocation when
// the pinned count reaches 0. Since we want the continuation of the
// delete operation to be invoked only once, we check if the block
// address that it is called for is equal to the base address of the
// allocation.
static int
_agas_local_free_async_handler(hpx_addr_t base, hpx_addr_t addr,
                               hpx_addr_t rsync) {
  gva_t gva = { .addr = addr };
  agas_t *agas = (agas_t*)here->gas;

  void *lva = btt_lookup(agas->btt, gva);

  // 1) remove this mapping
  btt_remove(agas->btt, gva);

  if (addr == base) {
    if (lva) {
      global_free(lva);
    }

    hpx_lco_error(rsync, HPX_SUCCESS, HPX_NULL);
  }
  return HPX_SUCCESS;
}
HPX_ACTION(HPX_DEFAULT, 0, _agas_local_free_async,
           _agas_local_free_async_handler, HPX_ADDR, HPX_ADDR, HPX_ADDR);

hpx_parcel_t *_new_parcel(hpx_addr_t addr, hpx_action_t action, int n, ...) {
  va_list args;
  va_start(args, n);
  hpx_parcel_t *p = action_parcel_create(addr, action, HPX_NULL,
                                         HPX_ACTION_NULL, n, &args);
  va_end(args);
  return p;
}

void agas_local_free(agas_t *agas, gva_t gva, void *lva, hpx_addr_t rsync) {

  // how many blocks are involved in this mapping?
  size_t blocks = btt_get_blocks(agas->btt, gva);
  uint32_t bsize = 1 << gva.bits.size;
  hpx_addr_t base = gva.addr;

  for (int i = 0; i < blocks; ++i) {
    hpx_parcel_t *p = _new_parcel(gva.addr, _agas_local_free_async, 3,
                                  &base, &gva.addr, &rsync);
    btt_try_delete(agas->btt, gva, p);
    gva.bits.offset += bsize;
  }
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
