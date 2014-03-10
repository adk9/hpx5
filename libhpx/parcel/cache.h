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
#ifndef LIBHPX_PARCEL_CACHE_H
#define LIBHPX_PARCEL_CACHE_H

#include "parcel.h"
#include "attributes.h"

/// A hashtable cache of parcels. Each entry in the table is a freelist of
/// parcels of a single size.

typedef struct cache cache_t;

/// Allocate a new cache.
///
/// @param capindex - the capacity index used for the initial cache, the actual
///                   capacity of the cache is implementation-specific (see
///                   cache.c)
HPX_INTERNAL cache_t *cache_new(int capindex);

/// Delete a cache.
///
/// This frees all of the resources associated with the cache, including any
/// parcels that this cache allocated to satisfy a get.
///
/// @param cache - the cache to delete
HPX_INTERNAL void cache_delete(cache_t *cache) HPX_NON_NULL(1);

/// Get a parcel from the cache.
///
/// This will allocate a new parcel, if no parcels that match @p size are in the
/// cache. The cache uses a block allocator.
///
/// @param cache - the cache to allocate from
/// @param  size - the size of the parcel we want
/// @returns     - an inplace parcel that is at least big enough for the allocation
HPX_INTERNAL hpx_parcel_t *cache_get(cache_t *cache, int size) HPX_NON_NULL(1);

/// Return a parcel to the cache.
///
/// @param  cache - the cache to return the parcel to
/// @param parcel - the parcel to return
HPX_INTERNAL void cache_put(cache_t *cache, hpx_parcel_t *parcel) HPX_NON_NULL(1, 2);

#endif
