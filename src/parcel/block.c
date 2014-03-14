/*
  ====================================================================
  High Performance ParalleX Library (libhpx)
  
  ParcelQueue Functions
  src/parcel/block.c

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
  ====================================================================
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "libhpx/parcel.h"                      /* hpx_parcel_t */
#include <stdlib.h>
#include <strings.h>                            /* bzero */
#include "block.h"
#include "clz.h"
#include "debug.h"
#include "padding.h"                            /* PAD_TO_CACHELINE */


hpx_parcel_t *block_new(parcel_block_t *next, int payload_size) {
  int parcel_size = sizeof(hpx_parcel_t) + payload_size;
  int space = HPX_PAGE_SIZE - sizeof(*next);
  int n = (parcel_size > space) ? 1 : space / parcel_size;
  dbg_logf("Allocating a parcel block of %i %i-byte parcels (payload %i)\n",
           n, parcel_size, payload_size);
    
  int bytes = sizeof(parcel_block_t) + n * parcel_size;
  parcel_block_t *b = valloc(bytes);
  if (!b) {
    __hpx_errno = HPX_ERROR_NOMEM;
    dbg_printf("Block allocation failed for payload size %d\n", payload_size);
    return NULL;
  }
  bzero(b, bytes);
  b->header.max_payload_size = payload_size;
  b->header.pinned = false;
  b->header.next = next;

  /* initialize the parcels we just allocated, chaining them together */
  hpx_parcel_t *prev = NULL;
  hpx_parcel_t *curr = NULL;
  for (int i = 0; i < n; ++i) {
    curr = (hpx_parcel_t*)&b->data[i * parcel_size];
    curr->data = &curr->payload;
    curr->next = prev;
    prev = curr;
  }
  
  /* return the last parcel, which is the head of the freelist now */
  return curr;
}

void block_delete(parcel_block_t *chain) {
  if (!chain)
    return;

  for (parcel_block_t *top = chain; chain; top = chain) {
    chain = top->header.next;
    free(top);
  }
}

parcel_block_t *get_block(hpx_parcel_t *parcel) {
  const uintptr_t MASK = ~0 << ctz(HPX_PAGE_SIZE);
  return (parcel_block_t*)((uintptr_t)parcel & MASK);
}

int get_block_size(parcel_block_t *block) {
  int payload_size = block->header.max_payload_size;
  int parcel_size = sizeof(hpx_parcel_t) + payload_size;
  int space = HPX_PAGE_SIZE - sizeof(*block);
  if (parcel_size < space)
    return space + sizeof(*block);
  else
    return parcel_size + sizeof(*block);
}

int block_payload_size(hpx_parcel_t *parcel) {
  dbg_assert_precondition(parcel);
  parcel_block_t *block = get_block(parcel);
  return block->header.max_payload_size;
}
