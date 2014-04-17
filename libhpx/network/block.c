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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "hpx/builtins.h"
#include "libhpx/debug.h"
#include "libhpx/parcel.h"
#include "block.h"
#include "padding.h"

#define BLOCK_SIZE 4 * HPX_PAGE_SIZE


/// Local max function.
static int _max(int lhs, int rhs) {
  return (lhs > rhs) ? lhs : rhs;
}


/// The block header structure
///
/// This takes up the front of the block, and contains metadata about the block.
typedef struct {
  int max_payload_size;
  int block_size;
  bool pinned;
  hpx_parcel_t *free;
  block_t *next;                                // for garbage collection
} header_t;


/// The block itself.
///
/// All parcels in the block are both cache-line aligned, and padded to
/// cache-lines.
struct block {
  header_t header;
  const char padding[PAD_TO_CACHELINE(sizeof(header_t))];
  char parcels[];
};


/// When we allocate a new block we need to:
///   1) compute the padded parcel size to allocate
///   2) compute how many parcels we can fit in the block
///   3) malloc the block itself
///   4) initialize the block header
///   5) assemble the block's freelist
///   6) initialize the parcels as needed
block_t *block_new(int size) {
  // 1) make sure there's at least one word
  int parcel_size = sizeof(hpx_parcel_t) + size;
  int padding = PAD_TO_CACHELINE(parcel_size);
  int padded_size = parcel_size + padding;
  if (padded_size - sizeof(hpx_parcel_t) < sizeof(void*)) {
    dbg_error("free space expected, check sizeof(hpx_parcel_t).\n");
    return NULL;
  }

  // 2)
  int space = BLOCK_SIZE - sizeof(block_t);
  int n = _max(1, space / padded_size);
  dbg_log("Allocating a parcel block of %i %i-byte parcels (payload %i)\n",
          n, padded_size, size);

  // 3) make sure the block is aligned correctly
  int block_size = sizeof(block_t) + n * padded_size;
  block_t *b = NULL;
  if (posix_memalign((void**)&b, BLOCK_SIZE, block_size)) {
    dbg_error("parcel block allocation failed for payload size %d\n", size);
    return NULL;
  }

  // 4)
  b->header.max_payload_size = padded_size;
  b->header.block_size       = block_size;
  b->header.pinned           = false;
  b->header.free             = NULL;
  b->header.next             = NULL;

  // 5 & 6)
  hpx_parcel_t *prev = NULL;
  for (int i = 0; i < n; ++i) {
    hpx_parcel_t *p = (hpx_parcel_t*)(b->parcels + (i * padded_size));
    parcel_push(&prev, p);
    prev = p;
  }
  b->header.free = prev;

  // return the block
  return b;
}


/// The block is just one contiguous chunk of space, and it didn't allocate
/// anything else out-of-place, so we can just free it.
void block_delete(block_t *block) {
  free(block);
}


hpx_parcel_t *block_get_free(const block_t *block) {
  return block->header.free;
}


bool block_is_pinned(const block_t *block) {
  return block->header.pinned;
}


void block_set_pinned(block_t *block, bool val) {
  block->header.pinned = val;
}


int block_get_size(const block_t *block) {
  return block->header.block_size;
}


int block_get_max_payload_size(const block_t *block) {
  return block->header.max_payload_size;
}


block_t *block_from_parcel(hpx_parcel_t *parcel) {
  const uintptr_t MASK = ~0 << ctzl(BLOCK_SIZE);
  return (block_t*)((uintptr_t)parcel & MASK);
}


void block_push(block_t** list, block_t *block) {
  block->header.next = *list;
  *list = block;
}


block_t *block_pop(block_t **list) {
  block_t *block = *list;
  *list = block->header.next;
  return block;
}
