/*
  ====================================================================
  High Performance ParalleX Library (libhpx)

  ParcelQueue Functions
  src/parcel/cache.h

  Copyright (c) 2013, Trustees of Indiana University
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
  ====================================================================
*/

#ifndef LIBHPX_PARCEL_CACHE_H_
#define LIBHPX_PARCEL_CACHE_H_

#include "hpx/parcel.h"
#include "hpx/system/attributes.h"

#define PARCEL_CACHE_INIT { 0, 0, NULL }

struct HPX_INTERNAL parcel_cache;
typedef struct parcel_cache parcel_cache_t;

/**
 * A hashtable cache of parcels. Each entry in the table is a stack of free
 * parcels.
 */
struct parcel_cache {
  int capindex;
  int capacity;
  hpx_parcel_t **table;
};

HPX_INTERNAL void cache_init(parcel_cache_t *cache);
HPX_INTERNAL void cache_fini(parcel_cache_t *cache);
HPX_INTERNAL hpx_parcel_t *cache_get(parcel_cache_t *cache, int payload);
HPX_INTERNAL void cache_put(parcel_cache_t *cache, hpx_parcel_t *parcel);
HPX_INTERNAL void cache_refill(parcel_cache_t *cache, hpx_parcel_t *parcel);

#endif
