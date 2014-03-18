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

#include "libhpx/debug.h"
#include "libhpx/parcel.h"
#include "cache.h"
#include "block.h"
#include "padding.h"


/// I have implemented a simple probed hashtable. This constant bounds the
/// number of probes that we do before we decide to expand the cache.
static const int PROBES = 2;


/// Each cache is a hashtable table of lists of parcels, keyed by the parcel's
/// aligned size.
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


/// Grow the cache.
///
/// This will increase the capacity of the cache according to the capacities
/// array, and then insert everything into the new cache. It may happen
/// recursively during this internal insert.
static void _expand(cache_t *cache);


/// Find the right list in the cache.
///
/// This may resize the cache if it detects a collision during the search. The
/// resulting list might be empty, but it won't be NULL.
static HPX_RETURNS_NON_NULL hpx_parcel_t **_find(cache_t *cache, int size) {
  int probes = PROBES;
  int i = size % cache->capacity;
  hpx_parcel_t *value = cache->table[i];

  // if a list already exists in this bucket, we need to determine if it's the
  // right size list, or if we need to probe (and possibly expand the cache)
  while (value) {

    // did we find an existing block?
    block_t *block = block_from_parcel(value);
    int max_payload = block_get_max_payload_size(block);
    if (max_payload == size)
      break;                                    // yes

    // have we probed too many times?
    if (!probes--) {
      _expand(cache);                           // yes, expand the cache and
      return _find(cache, size);                // retry the find
    }

    // probe to get a new bucket to check (i^2 % prime size)
    i = (i * i) % cache->capacity;
    value = cache->table[i];
  }

  // return the list that we found, by address so that it can be updated
  // directly
  return &cache->table[i];
}


/// Expand the table.
///
/// We use the static "capacities" array of small primes as hashtable sizes. See
/// the link in the comments above as to why.
static void _expand(cache_t *cache) {
  // remember the old table
  hpx_parcel_t **table = cache->table;
  int capacity = cache->capacity;

  // increase the size of the cache
  ++cache->capindex;

  // unrecoverable error if the cache is too big for our table
  // NB: this is an extremely unexpected situation---if this becomes a problem,
  //     there are three ways to fix it.
  //       1) increase PROBES and recompile
  //       2) increase the number of primes in capacities
  //       3) use a different hashtable implementation
  if (cache->capindex >= sizeof(capacities)) {
    dbg_error("could not expand a cache to size %i.\n", cache->capindex);
    hpx_abort(-1);
  }

  // allocate the new table
  cache->capacity = capacities[cache->capindex];
  cache->table = calloc(cache->capacity, sizeof(cache->table[0]));
  if (!cache->table) {
    dbg_error("failed to expand a cache table, %i.\n", cache->capacity);
    hpx_abort(-1);
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
cache_t *cache_new(void) {
  cache_t *cache = malloc(sizeof(*cache));
  if (!cache) {
    dbg_error("failed to allocate a parcel cache.\n");
    return NULL;
  }

  cache->blocks   = NULL;
  cache->capindex = 0;
  cache->capacity = capacities[cache->capindex];
  cache->table    = calloc(cache->capacity, sizeof(cache->table[0]));
  if (!cache->table) {
    dbg_error("failed to allocate a cache table, %i.\n", cache->capacity);
    free(cache);
    return NULL;
  }

  return cache;
}


/// delete a cache
void cache_delete(cache_t *cache) {
  if (!cache)
    return;

  if (cache->table)
    free(cache->table);

  while (cache->blocks)
    block_delete(block_pop(&cache->blocks));

  free(cache);
}


/// Find the list for the payload size requested.
hpx_parcel_t *cache_get(cache_t *cache, int size) {
  // we only allocate cache-line aligned parcels using this cache
  int parcel_size = sizeof(hpx_parcel_t) + size;
  int padding = PAD_TO_CACHELINE(parcel_size);
  int padded_size = parcel_size + padding;

  hpx_parcel_t **list = _find(cache, padded_size);
  hpx_parcel_t *p = parcel_pop(list);
  if (!p) {
    block_t *b = block_new(size);
    assert(b);
    parcel_cat(list, block_get_free(b));
    p = parcel_pop(list);
  }
  return p;
}


/// return a parcel to the cache
void cache_put(cache_t *cache, hpx_parcel_t *parcel) {
  block_t *block = block_from_parcel(parcel);
  int max_payload = block_get_max_payload_size(block);
  hpx_parcel_t **list = _find(cache, max_payload);
  parcel_push(list, parcel);
}
