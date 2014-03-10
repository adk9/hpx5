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

#include <stdlib.h>
#include "parcel.h"
#include "block.h"
#include "debug.h"
#include "padding.h"
#include "builtins.h"

static int _max(int lhs, int rhs) {
  return (lhs > rhs) ? lhs : rhs;
}


typedef struct {
  int max_payload_size;
  bool pinned;
  block_t *next;                                // for garbage collection
} header_t;


struct block {
  header_t header;
  const char padding[PAD_TO_CACHELINE(sizeof(header_t))];
  char parcels[];
};


hpx_parcel_t *
block_new(block_t **list, int size) {
  // compute the parcel size we want to allocate---we only allocate
  // cache-aligned size parcels at the moment.
  int parcel_size = sizeof(hpx_parcel_t) + size;
  int padding = PAD_TO_CACHELINE(parcel_size);
  int padded_size = parcel_size + padding;

  // compute how many parcels we can fit in a block (taking the size of the
  // block header into account, and given that we want to allocate one block per
  // page).
  int space = HPX_PAGE_SIZE - sizeof(block_t);
  int n = _max(1, space / padded_size);
  dbg_log("Allocating a parcel block of %i %i-byte parcels (payload %i)\n",
          n, padded_size, size);

  // allocate the block---it might be more than one page big if padded_size is
  // large, but it's always a page aligned address so that we can find the block
  // header for any parcel
  int block_size = sizeof(block_t) + n * padded_size;
  block_t *b = NULL;
  if (posix_memalign((void**)&b, HPX_PAGE_SIZE, block_size)) {
    dbg_error("parcel block allocation failed for payload size %d\n", size);
    return NULL;
  }

  // initialize the block header
  b->header.max_payload_size = padded_size;
  b->header.pinned = false;
  b->header.next = *list;
  *list = b;

  // initialize the parcels we just allocated, chaining them together---we only
  // allocate in-place parcels right now
  hpx_parcel_t *prev = NULL;
  for (int i = 0; i < n; ++i) {
    hpx_parcel_t *p = (hpx_parcel_t*)(b->parcels + (i * padded_size));
    parcel_set_inplace(p);
    parcel_set_next(p, prev);
    prev = p;
  }

  // return the last parcel, which is the head of the freelist now
  return prev;
}


void
block_delete(block_t *block) {
  if (!block)
    return;

  block_t *next = block->header.next;
  free(block);
  block_delete(next);
}


block_t *
get_block(hpx_parcel_t *parcel) {
  const uintptr_t MASK = ~0 << ctzl(HPX_PAGE_SIZE);
  return (block_t*)((uintptr_t)parcel & MASK);
}


// int
// get_block_size(block_t *block) {
//   int payload_size = block->header.max_payload_size;
//   int parcel_size = sizeof(hpx_parcel_t) + payload_size;
//   int space = HPX_PAGE_SIZE - sizeof(*block);
//   return _max(parcel_size, space);
// }


int
block_payload_size(hpx_parcel_t *parcel) {
  block_t *block = get_block(parcel);
  return block->header.max_payload_size;
}
