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
#ifndef LIBHPX_NETWORK_PARCEL_BLOCK_H
#define LIBHPX_NETWORK_PARCEL_BLOCK_H

#include "hpx/hpx.h"

/// ----------------------------------------------------------------------------
/// @file libhpx/parcel/block.h
/// @brief Represents a block of parcels.
///
/// Parcels are allocated in blocks.
/// ----------------------------------------------------------------------------
typedef struct block block_t;


/// ----------------------------------------------------------------------------
/// Allocate a new block of parcels.
///
/// The newly allocated block is essentially a linked list of all of the
/// allocated parcels, which can be retrieved using block_head().
///
/// @param size - the minimum size of the payload to allocate, max payload size
///               for the block parcels will be padded to a cache-line.
/// ----------------------------------------------------------------------------
HPX_INTERNAL block_t *block_new(int size);


/// ----------------------------------------------------------------------------
/// Delete the block of parcels.
///
/// This implicitly "frees" all of the parcels that the allocation of this block
/// created, so using a parcel from a deleted block is a use-after-free
/// error. This is unchecked for performance reasons.
/// ----------------------------------------------------------------------------
HPX_INTERNAL void block_delete(block_t *block)
  HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// Get the head of the block's freelist.
/// ----------------------------------------------------------------------------
HPX_INTERNAL hpx_parcel_t *block_get_free(const block_t *block)
  HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// Check to see if the block has been pinned.
/// ----------------------------------------------------------------------------
HPX_INTERNAL bool block_is_pinned(const block_t *block)
  HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// Set the block's pinned state.
/// ----------------------------------------------------------------------------
HPX_INTERNAL void block_set_pinned(block_t *block, bool val)
  HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// Get the total number of bytes in the block.
/// ----------------------------------------------------------------------------
HPX_INTERNAL int block_get_size(const block_t *block)
  HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// Get the maximum payload size of parcels in a block.
/// ----------------------------------------------------------------------------
HPX_INTERNAL int block_get_max_payload_size(const block_t *block)
  HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// Get the block that a parcel was allocated in.
/// ----------------------------------------------------------------------------
HPX_INTERNAL block_t *block_from_parcel(hpx_parcel_t *parcel)
  HPX_NON_NULL(1) HPX_RETURNS_NON_NULL;


/// ----------------------------------------------------------------------------
/// Push a block onto a block list.
/// ----------------------------------------------------------------------------
HPX_INTERNAL void block_push(block_t** list, block_t *block)
  HPX_NON_NULL(1, 2);


/// ----------------------------------------------------------------------------
/// Pop a block from a list of blocks.
/// ----------------------------------------------------------------------------
HPX_INTERNAL block_t *block_pop(block_t **list)
  HPX_NON_NULL(1);



#endif // LIBHPX_NETWORK_PARCEL_BLOCK_H
