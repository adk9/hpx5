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

#include <string.h>
#include <jemalloc/jemalloc_global.h>
#include <hpx/builtins.h>
#include <libhpx/locality.h>
#include <libhpx/action.h>

#include "gpa.h"
#include "heap.h"
#include "pgas.h"

static HPX_ACTION_DECL(_calloc_init);
static HPX_ACTION_DECL(_set_csbrk);

/// Internal utility actions for dealing with calloc.
/// @{
typedef struct {
  uint64_t offset;
  uint32_t  bytes;
  uint32_t  bsize;
} _calloc_init_args_t;


/// Allocate from the cyclic space.
///
/// This is performed at the single cyclic server node (usually rank 0), and
/// doesn't need to be broadcast because the server controls this for
/// everyone. All global cyclic allocations are rooted at rank 0.
///
/// @param        n The number of blocks to allocate.
/// @param    bsize The block size for this allocation.
///
/// @returns The base address of the global allocation.
hpx_addr_t pgas_cyclic_alloc_sync(size_t n, uint32_t bsize) {
  uint64_t offset = heap_alloc_cyclic(global_heap, n, bsize);
  assert(offset != 0);

  uint64_t csbrk = heap_get_csbrk(global_heap);
  hpx_addr_t sync = hpx_lco_future_new(0);
  hpx_bcast(_set_csbrk, sync, &csbrk, sizeof(csbrk));
  hpx_lco_wait(sync);
  hpx_lco_delete(sync, HPX_NULL);

  hpx_addr_t addr = pgas_offset_to_gpa(here->rank, offset);
  DEBUG_IF(addr == HPX_NULL) {
    dbg_error("should not get HPX_NULL during allocation\n");
  }
  return addr;
}

static int _cyclic_alloc_handler(size_t n, size_t bsize) {
  hpx_addr_t addr = pgas_cyclic_alloc_sync(n, bsize);
  HPX_THREAD_CONTINUE(addr);
}
HPX_ACTION_DEF(DEFAULT, _cyclic_alloc_handler, pgas_cyclic_alloc, HPX_SIZE_T,
               HPX_SIZE_T);

/// Allocate zeroed memory from the cyclic space.
///
/// This is performed at the single cyclic server node (usually rank 0) and is
/// broadcast to all of the ranks in the system using hpx_bcast(). It waits for
/// the broadcast to finish before returning. All global cyclic allocations are
/// rooted at rank 0.
///
/// @param        n The number of blocks to allocate.
/// @param    bsize The block size for this allocation.
///
/// @returns The base address of the global allocation.
hpx_addr_t pgas_cyclic_calloc_sync(size_t n, uint32_t bsize) {
  assert(here->rank == 0);

  // Figure out how many blocks ber node that we need, and then allocate that
  // much cyclic space from the heap.
  uint64_t offset = heap_alloc_cyclic(global_heap, n, bsize);

  // We broadcast the csbrk to the system to make sure that people can do
  // effective heap_is_cyclic tests.
  uint64_t csbrk = heap_get_csbrk(global_heap);
  hpx_addr_t sync = hpx_lco_future_new(0);
  hpx_bcast(_set_csbrk, sync, &csbrk, sizeof(csbrk));
  hpx_lco_wait(sync);
  hpx_lco_delete(sync, HPX_NULL);

  // Broadcast the calloc so that each locality can zero the correct memory.
  _calloc_init_args_t args = {
    .offset = offset,
    .bytes  = n,
    .bsize  = bsize
  };

  sync = hpx_lco_future_new(0);
  hpx_bcast(_calloc_init, sync, &args, sizeof(args));
  hpx_lco_wait(sync);
  hpx_lco_delete(sync, HPX_NULL);

  hpx_addr_t addr = pgas_offset_to_gpa(here->rank, offset);
  DEBUG_IF(addr == HPX_NULL) {
    dbg_error("should not get HPX_NULL during allocation\n");
  }
  return addr;
}

static int _cyclic_calloc_handler(size_t n, size_t bsize) {
  hpx_addr_t addr = pgas_cyclic_calloc_sync(n, bsize);
  HPX_THREAD_CONTINUE(addr);
}
HPX_ACTION_DEF(DEFAULT, _cyclic_calloc_handler, pgas_cyclic_calloc, HPX_SIZE_T,
               HPX_SIZE_T);


/// This is the hpx_call_* target for doing a calloc initialization.
///
/// This essentially scans through the blocks on a particular rank associated
/// with the offset, and memsets them to 0. We can't just do one large memset
/// because we have alignment issues and may have internal padding.
///
/// @param         args The arguments to the initializer include the base offset
///                     of the allocation (i.e., the base block id), as well as
///                     the number of blocks and the size of each block.
///
/// @returns HPX_SUCCESS
static HPX_ACTION(_calloc_init, _calloc_init_args_t *args) {
  // Create a global physical address from the offset so that we can perform
  // cyclic address arithmetic on it. This avoids any issues with internal
  // padding, since the addr_add already needs to be able to deal with that
  // correctly.
  //
  // Then compute the gpa for each local block, convert it to an lva, and then
  // memset it.
  uint32_t blocks = ceil_div_64(args->bytes, here->ranks);
  hpx_addr_t gpa = pgas_offset_to_gpa(here->rank, args->offset);
  for (int i = 0, e = blocks; i < e; ++i) {
    void *lva = pgas_gpa_to_lva(gpa);
    memset(lva, 0, args->bsize);
    // increment the global address by one cycle
    gpa = hpx_addr_add(gpa, args->bsize * here->ranks, args->bsize);
  }
  return HPX_SUCCESS;
}


HPX_ACTION(pgas_free, void) {
  hpx_addr_t gpa = hpx_thread_current_target();
  if (here->rank != pgas_gpa_to_rank(gpa)) {
    dbg_error("PGAS free operation for rank %u arrived at rank %u instead.\n",
              pgas_gpa_to_rank(gpa), here->rank);
    return HPX_ERROR;
  }
  void *lva = pgas_gpa_to_lva(gpa);
  libhpx_global_free(lva);
  return HPX_SUCCESS;
}


static HPX_ACTION(_set_csbrk, size_t *offset) {
  int e = heap_set_csbrk(global_heap, *offset);
  dbg_check(e, "cyclic allocation ran out of memory at rank %u", here->rank);
  return e;
}

static int _memput_rsync_handler(int src, uint64_t command) {
  hpx_addr_t rsync = pgas_offset_to_gpa(src, command);
  hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
  return HPX_SUCCESS;
}
HPX_ACTION_DEF(DEFAULT, _memput_rsync_handler, memput_rsync, HPX_INT,
               HPX_UINT64);

static int _lco_set_handler(int src, uint64_t command) {
  hpx_addr_t lco = pgas_offset_to_gpa(here->rank, command);
  hpx_lco_set(lco, 0, NULL, HPX_NULL, HPX_NULL);
  return HPX_SUCCESS;
}
HPX_ACTION_DEF(INTERRUPT, _lco_set_handler, lco_set, HPX_INT, HPX_UINT64);
