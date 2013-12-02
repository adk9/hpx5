/*
  ====================================================================
  High Performance ParalleX Library (libhpx)
  
  ParcelQueue Functions
  src/parcel/cache.c

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
#include "block.h"                              /* block_payload_size */
#include "cache.h"
#include "debug.h"
#include "parcel.h"

/**
 * From http://planetmath.org/goodhashtableprimes. Used to resize our cache of
 * parcel freelists when we the load gets too high.
 */
static const int capacities[] = {
  53, 97, 193, 389, 769, 1543, 3079, 6151, 12289, 24593, 49157, 98317,
  196613, 393241, 786433, 1572869, 3145739, 6291469, 12582917, 25165843,
  50331653, 100663319, 201326611, 402653189, 805306457, 1610612741
};

/**
 * Initialization and finalization.
 * @{
 */
void cache_init(parcel_cache_t *cache) {
  dbg_assert_precondition(cache);
  cache->table = calloc(capacities[cache->capacity], sizeof(cache->table[0]));
}

void cache_fini(parcel_cache_t *cache) {
  dbg_assert_precondition(cache);
  free(cache->table);
}

static hpx_parcel_t *pop(hpx_parcel_t **parcel) {
  hpx_parcel_t *p = *parcel;
  *parcel = (*parcel)->next;
  return p;
}

hpx_parcel_t *cache_get(parcel_cache_t *cache, int bytes) {
  dbg_assert_precondition(cache);
  dbg_assert_precondition(cache->table);
  hpx_parcel_t **stack = &cache->table[bytes % capacities[cache->capacity]];
  if (!*stack)
    return NULL;

  int payload_size = block_payload_size(*stack);
  if (bytes != payload_size) {
    dbg_logf("Collision in cache between %i and %i", bytes, payload_size); 
    return NULL;
  }
  
  return pop(stack);
}

static void push(hpx_parcel_t **bucket, hpx_parcel_t *parcel) {
  dbg_assert_precondition(bucket);
  dbg_assert_precondition(parcel);
  parcel->next = *bucket;
  *bucket = parcel->next;
}

void cache_put(parcel_cache_t *cache, hpx_parcel_t *parcel) {
  dbg_assert_precondition(cache);
  dbg_assert_precondition(cache->table);
  dbg_assert_precondition(parcel);
  int cap = capacities[cache->capacity];
  int size = block_payload_size(parcel);
  hpx_parcel_t **bucket = &cache->table[size % cap];
  assert(!*bucket || (size == block_payload_size(*bucket)) && "collision");
  assert(!*bucket || !parcel->next && "Cannot concat bucket list yet.");
  push(bucket, parcel);
}
