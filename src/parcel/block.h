/*
  ====================================================================
  High Performance ParalleX Library (libhpx)
  
  ParcelQueue Functions
  src/parcel/block.h

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
  ====================================================================
*/

#ifndef LIBHPX_PARCEL_BLOCK_H_
#define LIBHPX_PARCEL_BLOCK_H_

#include "hpx/parcel.h"
#include "hpx/system/attributes.h"
#include "padding.h"                  /* PAD_TO_CACHELINE */

struct parcel_block {
  struct header {
    int max_payload_size;
    bool pinned;
    struct parcel_block *next;
  } header;
  const char padding[PAD_TO_CACHELINE(sizeof(struct header))];
  char data[];
};

typedef struct HPX_INTERNAL parcel_block parcel_block_t;

HPX_INTERNAL hpx_parcel_t *block_new(parcel_block_t *next, int size);
HPX_INTERNAL void block_delete(parcel_block_t *block);
HPX_INTERNAL parcel_block_t *get_block(hpx_parcel_t *parcel);
HPX_INTERNAL int get_block_size(parcel_block_t *block);
HPX_INTERNAL int block_payload_size(hpx_parcel_t *parcel);

#endif /* LIBHPX_PARCEL_BLOCK_H_ */
