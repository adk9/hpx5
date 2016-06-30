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
#include <libhpx/scheduler.h>
#include "agas.h"
#include "btt.h"
#include "gva.h"

static int _insert_btt_handler(hpx_addr_t addr, int rank, size_t n,
                               uint32_t attr) {
  agas_t *agas = (agas_t*)here->gas;
  gva_t gva = { .addr = addr };
  dbg_assert(rank != here->rank);
  btt_upsert(agas->btt, gva, rank, NULL, n, attr);
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, 0, _insert_btt, _insert_btt_handler,
                     HPX_ADDR, HPX_INT, HPX_SIZE_T, HPX_UINT32);

static int
_alloc_user_handler(void *UNUSED, hpx_addr_t addr, size_t n, size_t padded,
                    uint32_t attr, void *lva, int zero) {
  agas_t *agas = (agas_t*)here->gas;
  hpx_parcel_t *p = scheduler_current_parcel();

  if (p->src != here->rank) {
    size_t boundary = (padded < 8) ? 8 : padded;
    int e = posix_memalign(&lva, boundary, padded);
    dbg_check(e, "Failed memalign\n");
    (void)e;
  }

  if (zero) {
    lva = memset(lva, 0, padded);
  }

  gva_t gva = { .addr = addr };
  btt_upsert(agas->btt, gva, here->rank, lva, n, attr);

  if (p->src != here->rank) {
    hpx_addr_t src = hpx_thread_current_cont_target();
    return hpx_call_cc(src, _insert_btt, &addr, &here->rank, &n, &attr);
  }
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_PINNED, _alloc_user, _alloc_user_handler,
                     HPX_POINTER, HPX_ADDR, HPX_SIZE_T, HPX_SIZE_T, HPX_UINT32,
                     HPX_POINTER, HPX_INT);

static hpx_addr_t
_agas_alloc_user(size_t n, uint32_t bsize, hpx_gas_dist_t dist,
                 uint32_t attr, int zero) {
  agas_t *agas = (agas_t*)here->gas;
  dbg_assert(bsize <= agas->vtable.max_block_size);

  // determine the padded block size.
  uint32_t  align = ceil_log2_32(bsize);
  dbg_assert(align <= 32);
  size_t padded = 1u << align;

  agas_alloc_bsize = padded;
  // Allocate the blocks as a contiguous, aligned array from local memory.
  void *lva = global_memalign(padded, n * padded);
  if (!lva) {
    dbg_error("failed user-defined allocation\n");
  }

  gva_t gva = agas_lva_to_gva(agas, lva, padded);
  hpx_addr_t base = gva.addr;
  hpx_addr_t done = hpx_lco_and_new(n);
  for (int i = 0; i < n; ++i) {
    hpx_addr_t where = dist(i, n, bsize);
    hpx_call(where, _alloc_user, done, &gva, &n, &padded, &attr, &lva, &zero);
    lva += padded;
    gva.bits.offset += padded;
  }
  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);
  return base;
}

hpx_addr_t agas_alloc_user(size_t n, size_t bbsize, uint32_t boundary,
                           hpx_gas_dist_t dist, uint32_t attr) {
  uint32_t bsize = bbsize;
  hpx_addr_t addr = _agas_alloc_user(n, bsize, dist, attr, 0);
  dbg_assert_str(addr != HPX_NULL, "HPX_NULL is not a valid allocation\n");
  return addr;
}

hpx_addr_t agas_calloc_user(size_t n, size_t bbsize, uint32_t boundary,
                            hpx_gas_dist_t dist, uint32_t attr) {
  uint32_t bsize = bbsize;
  hpx_addr_t addr = _agas_alloc_user(n, bsize, dist, attr, 0);
  dbg_assert_str(addr != HPX_NULL, "HPX_NULL is not a valid allocation\n");
  return addr;
}
