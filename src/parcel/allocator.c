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
#include "block.h"
#include "cache.h"
#include "ctx.h"                                /* ctx_add_kthread_init/fini */
#include "debug.h"
#include "padding.h"                            /* PAD_TO_CACHELINE */
#include "parcel.h"                             /* struct hpx_parcel */
#include "sync/locks.h"

/** Data */
static parcel_block_t             *blocks = NULL;
static parcel_cache_t             parcels = PARCEL_CACHE_INIT;
static __thread parcel_cache_t my_parcels = PARCEL_CACHE_INIT;

static int local_hits = 0;
static int global_hits = 0;
static int allocations = 0;

static void init(void *unused) {
  cache_init(&my_parcels);
}

static void fini(void *unused) {
  cache_fini(&my_parcels);
}

int parcel_allocator_init(struct hpx_context *ctx) {
  ctx_add_kthread_init(ctx, init, NULL);
  ctx_add_kthread_fini(ctx, fini, NULL);
  cache_init(&parcels);
  return HPX_SUCCESS;
}

int parcel_allocator_fini(void) {
  cache_fini(&parcels);
  block_delete(blocks);
  if (HPX_DEBUG)
    dbg_printf("Allocations: %i local hits, %i global hits, %i allocations\n",
               local_hits, global_hits, allocations);
  return HPX_SUCCESS;
}

hpx_parcel_t *parcel_allocator_get(int payload) {
  int size = sizeof(hpx_parcel_t) + payload;
  int aligned_size = payload + PAD_TO_CACHELINE(size);

  dbg_logf("Getting a parcel of payload size %i (aligned payload size %i)... ",
           payload, aligned_size); 
  
  hpx_parcel_t *p = cache_get(&my_parcels, aligned_size);

  if (!p) {
    dbg_logf("missed local cache, going to global... ");
    static tatas_lock_t lock = TATAS_INIT;
    tatas_acquire(&lock);
    {
      p = cache_get(&parcels, aligned_size);
      if (!p) {
        dbg_logf("missed global cache, allocating new block\n");
        if (HPX_DEBUG)
          sync_fadd(&allocations, 1, SYNC_ACQ_REL);
        p = block_new(blocks, aligned_size);
        cache_refill(&parcels, p);
        p = cache_get(&parcels, aligned_size);
      }
      else {
        dbg_logf("hit global cache\n");
        if (HPX_DEBUG)
          sync_fadd(&global_hits, 1, SYNC_ACQ_REL);
      }
    }
    tatas_release(&lock);
  }
  else {
    dbg_logf("hit local cache\n");
    if (HPX_DEBUG)
      sync_fadd(&local_hits, 1, SYNC_ACQ_REL);
  }
  
  p->size   = payload;
  p->action = HPX_ACTION_NULL;
  p->target = HPX_NULL;
  p->cont   = HPX_NULL;
  bzero(&p->payload, aligned_size);
  return p;
}

void parcel_allocator_put(hpx_parcel_t *parcel) {
  dbg_assert_precondition(parcel);
  dbg_logf("Returning parcel to local cache, payload size %i "
           "(aligned payload size %i)\n", parcel_get_data_size(parcel),
           block_payload_size(parcel));
  cache_put(&my_parcels, parcel);
}
