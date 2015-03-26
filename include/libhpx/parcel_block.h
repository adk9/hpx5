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
#ifndef LIBHPX_PARCEL_BLOCK_H
#define LIBHPX_PARCEL_BLOCK_H

#include <stddef.h>

struct hpx_parcel;
typedef struct parcel_block parcel_block_t;

parcel_block_t *parcel_block_new(size_t align, size_t n, size_t *offset)
  HPX_INTERNAL;
void parcel_block_delete(parcel_block_t *block)
  HPX_INTERNAL;
void *parcel_block_at(parcel_block_t *block, size_t offset)
  HPX_INTERNAL;
void parcel_block_deduct(parcel_block_t *block, size_t bytes)
  HPX_INTERNAL;
void parcel_block_delete_parcel(struct hpx_parcel *p)
  HPX_INTERNAL;

#endif // LIBHPX_PARCEL_BLOCK_H
