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
#ifndef LIBHPX_PARCEL_BLOCK_H
#define LIBHPX_PARCEL_BLOCK_H

#include "hpx.h"

/// ----------------------------------------------------------------------------
/// @file libhpx/parcel/block.h
/// @brief Represents a block of parcels.
///
/// Parcels are allocated in blocks.
/// ----------------------------------------------------------------------------
typedef struct block block_t;

HPX_INTERNAL hpx_parcel_t *block_new(block_t **list, int size);
HPX_INTERNAL void block_delete(block_t *block);
HPX_INTERNAL int block_size(block_t *block);
HPX_INTERNAL int block_payload_size(hpx_parcel_t *parcel);

HPX_INTERNAL block_t *parcel_get_block(hpx_parcel_t *parcel);

#endif // LIBHPX_PARCEL_BLOCK_H
