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

typedef struct HPX_INTERNAL parcel_block parcel_block_t;
HPX_INTERNAL hpx_parcel_t *block_new(parcel_block_t *next, int size);
HPX_INTERNAL void block_delete(parcel_block_t *block);
HPX_INTERNAL int block_payload_size(hpx_parcel_t *parcel);

#endif /* LIBHPX_PARCEL_BLOCK_H_ */
