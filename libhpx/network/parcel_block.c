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

#include <libsync/sync.h>
#include <hpx/builtins.h>
#include <libhpx/debug.h>
#include <libhpx/memory.h>
#include <libhpx/padding.h>
#include <libhpx/parcel_block.h>

struct parcel_block {
  size_t remaining;
  PAD_TO_CACHELINE(sizeof(size_t));
  char bytes[];
};

_HPX_ASSERT(sizeof(parcel_block_t) == HPX_CACHELINE_SIZE, block_header_size);

parcel_block_t *parcel_block_new(size_t align, size_t n, size_t *offset) {
  size_t bytes = n - sizeof(parcel_block_t);
  dbg_assert(bytes < n);
  parcel_block_t *block = registered_memalign(align, n);
  block->remaining = bytes;
  *offset = offsetof(parcel_block_t, bytes);
  return block;
}

void parcel_block_delete(parcel_block_t *block) {
  if (block->remaining != 0) {
    log_parcel("block freed with %zu bytes remaining\n", block->remaining);
  }
  registered_free(block);
}

void *parcel_block_at(parcel_block_t *block, size_t offset) {
  return &block->bytes[offset - sizeof(*block)];
}

void parcel_block_deduct(parcel_block_t *block, size_t bytes) {
  dbg_assert(bytes < SIZE_MAX/2);
  log("deducting %zu bytes from parcel block %p\n", bytes, (void*)block);
  size_t r = sync_fadd(&block->remaining, -bytes, SYNC_ACQ_REL) - bytes;
  if (!r) {
    parcel_block_delete(block);
  }
}
