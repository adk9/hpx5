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
#include "cache.h"
#include "block.h"
#include "debug.h"
#include "padding.h"
#include "parcel.h"

/// each cache is a hashtable table of lists of parcels, keyed by the parcel's
/// aligned size
struct cache {
  block_t *blocks;
  int capindex;
  int capacity;
  hpx_parcel_t **table;
};

// From http://planetmath.org/goodhashtableprimes. Used to resize our cache of
// parcel freelists when we the load gets too high.
static const int capacities[] = {
  53, 97, 193, 389, 769, 1543, 3079, 6151, 12289, 24593, 49157, 98317,
  196613, 393241, 786433, 1572869, 3145739, 6291469, 12582917, 25165843,
  50331653, 100663319, 201326611, 402653189, 805306457, 1610612741
};


/// grow the cache
static void _expand(cache_t *cache);


// find the right list in the cache, may resize the cache, returns non-null
static hpx_parcel_t **_find(cache_t *cache, int size) {
  int probes = 2;
  int i = size % cache->capacity;
  hpx_parcel_t *value = cache->table[i];

  while (value && (block_payload_size(value) != size)) {
    // if we didn't find a parcel fast enough, expand the table and perform the
    // get again
    if (!probes--) {
      _expand(cache);
      return _find(cache, size);
    }

    // otherwise probe
    i = (i * i) % cache->capacity;
    value = cache->table[i];
  }

  return &cache->table[i];
}


/// expand the table (might recursively expand the table)
static void _expand(cache_t *cache) {
  // remember the old table
  hpx_parcel_t **table = cache->table;
  int capacity = cache->capacity;

  // increase the size of the cache
  ++cache->capindex;
  if (cache->capindex >= sizeof(capacities)) {
    dbg_error("could not expand a cache to size %i.\n", cache->capindex);
  }

  cache->capacity = capacities[cache->capindex];
  cache->table = calloc(cache->capacity, sizeof(cache->table[0]));
  if (!cache->table) {
    dbg_error("failed to expand a cache table, %i.\n", cache->capacity);
    hpx_abort(1);
  }

  // insert anything from the old table into the new table
  for (int i = 0; i < capacity; ++i) {
    hpx_parcel_t *p = table[i];
    if (p)
      cache_put(cache, p);
  }

  // free the old table
  free(table);
}


/// allocate a cache, with an initial table size
cache_t*
cache_new(int capindex) {
  if (capindex >= sizeof(capacities)) {
    dbg_error("requested capacity is too large, %i.\n", capindex);
    return NULL;
  }

  cache_t *cache = malloc(sizeof(*cache));
  if (!cache) {
    dbg_error("failed to allocate a parcel cache.\n");
    return NULL;
  }

  cache->blocks = NULL;
  cache->capindex = capindex;
  cache->capacity = capacities[capindex];
  cache->table = calloc(cache->capacity, sizeof(cache->table[0]));
  if (!cache->table) {
    dbg_error("failed to allocate a cache table, %i.\n", cache->capacity);
    free(cache);
    return NULL;
  }

  return cache;
}


/// delete a cache
void
cache_delete(cache_t *cache) {
  if (!cache)
    return;
  if (cache->table)
    free(cache->table);
  if (cache->blocks)
    block_delete(cache->blocks);
  free(cache);
}


/// Find the list for the payload size requested.
hpx_parcel_t *
cache_get(cache_t *cache, int size) {
  // we only allocate cache-line aligned parcels using this cache
  int parcel_size = sizeof(hpx_parcel_t) + size;
  int padding = PAD_TO_CACHELINE(parcel_size);
  int padded_size = parcel_size + padding;

  hpx_parcel_t **list = _find(cache, padded_size);
  hpx_parcel_t *p = parcel_pop(list);
  if (!p) {
    parcel_cat(list, block_new(&cache->blocks, size));
    p = parcel_pop(list);
  }
  return p;
}


/// return a parcel to the cache
void
cache_put(cache_t *cache, hpx_parcel_t *parcel) {
  hpx_parcel_t **list = _find(cache, block_payload_size(parcel));
  parcel_push(list, parcel);
}
