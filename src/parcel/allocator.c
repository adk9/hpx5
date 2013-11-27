/*
  ====================================================================
  High Performance ParalleX Library (libhpx)
  
  ParcelQueue Functions
  src/parcel/allocator.c

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
  ====================================================================
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>                             /* malloc/free */
#include <strings.h>                            /* bzero */

#include "hpx/parcel.h"                         /* hpx_parcel_t */
#include "allocator.h"
#include "ctx.h"                                /* ctx_add_kthread_init/fini */
#include "debug.h"
#include "parcel.h"                             /* struct hpx_parcel */
#include "sync/locks.h"

/**
 * Given a number of bytes, how many bytes of padding do we need to get a size
 * that is a multiple of HPX_CACHELINE_SIZE? Macro because it's used in
 * structure definitions for padding.
 */
#define PAD_TO_CACHELINE(N_)                                            \
  ((HPX_CACHELINE_SIZE - (N_ % HPX_CACHELINE_SIZE)) % HPX_CACHELINE_SIZE)

static int parcel_get_aligned_size(int size);

/**
 * Block allocation of parcels.
 */
typedef struct block {
  struct header {
    int max_payload_size;
    struct block *next;
  } header;
  const char padding[PAD_TO_CACHELINE(sizeof(struct header))];
  char data[];
} block_t;

static hpx_parcel_t *allocate_parcel_block(block_t *next, int size);

/**
 * From http://planetmath.org/goodhashtableprimes. Used to resize our cache of
 * parcel freelists when we the load gets too high.
 */
static const int capacities[] = {
  53, 97, 193, 389, 769, 1543, 3079, 6151, 12289, 24593, 49157, 98317,
  196613, 393241, 786433, 1572869, 3145739, 6291469, 12582917, 25165843,
  50331653, 100663319, 201326611, 402653189, 805306457, 1610612741 };

/**
 * A hashtable cache of parcels. Each entry in the table is a stack of free
 * parcels.
 */
typedef struct {
  int capacity;                                 /*!< index into capacities[] */
  int size;                                     /*!< # used buckets */
  hpx_parcel_t *table;                          /*!< table of lists */
} cache_t;

static hpx_parcel_t *cache_get(cache_t *this, int bytes);
static void cache_put(cache_t *this, hpx_parcel_t *parcel);

/** Data */
static block_t             *blocks = NULL;
static tatas_lock_t parcels_lock   = TATAS_INIT;
static cache_t parcels             = { 0, 0, NULL };
static __thread cache_t my_parcels = { 0, 0, NULL };

hpx_parcel_t *parcel_get(int size) {
  int aligned_size = parcel_get_aligned_size(size);
  dbg_logf("Getting a parcel of payload size %i (aligned payload size %i)\n",
           size, aligned_size); 
  
  hpx_parcel_t *p = cache_get(&my_parcels, aligned_size);

  if (!p) {
    dbg_logf("Did not find one in local cache, checking global cache\n");
    tatas_acquire(&parcels_lock);
    p = cache_get(&parcels, aligned_size);
    if (!p) {
      dbg_logf("Did not find on in global cache, allocating block\n");
      p = allocate_parcel_block(blocks, aligned_size);
      cache_put(&parcels, p->next);
    }
    tatas_release(&parcels_lock);
  }
  
  p->size   = size;
  p->action = HPX_ACTION_NULL;
  p->target = HPX_NULL;
  p->cont   = HPX_NULL;
  bzero(&p->payload, aligned_size);
  return p;
}

void parcel_put(hpx_parcel_t *parcel) {
  cache_put(&my_parcels, parcel);
}

hpx_parcel_t *allocate_parcel_block(block_t *next, int payload_size) {
  int parcel_size = sizeof(hpx_parcel_t) + payload_size;
  int space       = HPX_PAGE_SIZE - sizeof(block_t);
  int n           = (parcel_size > space) ? 1 : space / parcel_size;
  dbg_logf("Allocating a parcel block of %i %i-byte parcels\n", n, parcel_size);
    
  int bytes       = sizeof(block_t) + n * parcel_size;
  
  block_t *b = valloc(bytes);
  if (!b) {
    __hpx_errno = HPX_ERROR_NOMEM;
    dbg_printf("Block allocation failed for payload size %d\n", payload_size);
    return NULL;
  }
  bzero(b, bytes);
  b->header.max_payload_size = payload_size;
  b->header.next = next;

  /* initialize the parcels we just allocated, chaining them together */
  hpx_parcel_t *prev = NULL;
  hpx_parcel_t *curr = NULL;
  for (int i = 0; i < n; ++i) {
    curr = (hpx_parcel_t*)&b->data[i * parcel_size];
    curr->data = &curr->payload;
    curr->next = prev;
    prev = curr;
  }
  
  /* return the last parcel, which is the head of the freelist now */
  return curr;
}

hpx_parcel_t *cache_get(cache_t *this, int bytes) {
  return NULL;
}

void cache_put(cache_t *this, hpx_parcel_t *parcel) {
}


/**
 * Initialization and finalization.
 * @{
 */
static void cache_initialize(cache_t *this) {
  dbg_assert_precondition(this);
  dbg_logf("Initializing parcels cache\n");
  this->table = calloc(capacities[this->capacity], sizeof(*this->table));
}

static void cache_finalize(cache_t *this) {
  dbg_assert_precondition(this);
  free(this->table);
}

static void my_parcels_initialize(void) {
  dbg_logf("Initializing my_parcels cache\n");
  cache_initialize(&my_parcels);
}

static void my_parcels_finalize(void) {
  cache_finalize(&my_parcels);
}

int parcel_allocator_initialize(struct hpx_context *ctx) {
  ctx_add_kthread_init(ctx, my_parcels_initialize);
  ctx_add_kthread_fini(ctx, my_parcels_finalize);
  
  cache_initialize(&parcels);
  return HPX_SUCCESS;
}

int parcel_allocator_finalize(void) {
  cache_finalize(&parcels);

  for (block_t *top = blocks; blocks != NULL; top = blocks) {
    blocks = top->header.next;
    free(top);
  }

  return HPX_SUCCESS;
}
/**
 * @}
 */

static int parcel_get_aligned_size(int size) {
    int bytes = sizeof(hpx_parcel_t) + size;
    return size + PAD_TO_CACHELINE(bytes);
}
