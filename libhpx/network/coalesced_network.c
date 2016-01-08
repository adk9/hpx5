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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <hpx/hpx.h>
#include <libsync/queues.h>
#include <libsync/sync.h>

#include <libhpx/action.h>
#include <libhpx/config.h>
#include <libhpx/debug.h>
#include <libhpx/gas.h>
#include <libhpx/libhpx.h>
#include <libhpx/network.h>
#include <libhpx/parcel.h>

typedef struct {
  network_t               vtable;
  network_t        *base_network;
  two_lock_queue_t         sends;
  uint64_t          parcel_count;
  uint64_t previous_parcel_count;
  uint64_t       coalescing_size;
  volatile uint64_t    syncflush;
} _coalesced_network_t;

static void _coalesced_network_delete(void *obj) {
  _coalesced_network_t *coalesced_network = obj;
  dbg_assert(coalesced_network);
  network_delete(coalesced_network->base_network);
  free(obj);
}

// demultiplexing action on the receiver side
static int _demux_handler(char* buffer, int n) {
  // retrieve the next parcel from the fat parcel
  while (n) {
    hpx_parcel_t *p = parcel_clone((void*)buffer);
    size_t bytes = parcel_size(p);
    buffer += bytes;
    n -= bytes;
    dbg_assert(n >= 0);
    parcel_launch(p);
  }
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _demux, _demux_handler,
                     HPX_POINTER, HPX_INT);

static void _send_n(_coalesced_network_t *network, uint64_t n_parcels) {
  // 0) Allocate temporary storage.
  struct {
    hpx_parcel_t *fatp;
    char         *next;
    int        n_bytes;
  } *locs = calloc(HPX_LOCALITIES, sizeof(*locs));

  // 1) We'll pull n parcels off the global send queue, and store them
  //    temporarily in a local stack, so that we can accumulate the number of
  //    bytes we need to send to each rank.
  gas_t *gas = here->gas;
  hpx_parcel_t *chain = NULL;
  hpx_parcel_t *p = NULL;
  while (n_parcels--) {
    p = sync_two_lock_queue_dequeue(&network->sends);
    size_t bytes = parcel_size(p);
    uint32_t   l = gas_owner_of(gas, p->target);
    locs[l].n_bytes += bytes;
    parcel_stack_push(&chain, p);
  }

  // 2) Allocate a parcel for each rank that we have bytes going to, and grab a
  //    pointer to the beginning of its buffer.
  for (int l = 0, e = HPX_LOCALITIES; l < e; ++l) {
    int n = locs[l].n_bytes;
    if (n) {
      locs[l].fatp = action_new_parcel(_demux, HPX_THERE(l), 0, 0, 2, NULL, n);
      locs[l].next = hpx_parcel_get_data(locs[l].fatp);
    }
  }

  // 3) Copy the chained parcels to the appropriate buffers.
  while ((p = parcel_stack_pop(&chain))) {
    size_t bytes = parcel_size(p);
    uint32_t   l = gas_owner_of(gas, p->target);
    memcpy(locs[l].next, p, bytes);
    locs[l].next += bytes;
    parcel_delete(p);
  }

  // 4) Send the fat parcel to each target.
  network_t *base = network->base_network;
  for (int l = 0, e = HPX_LOCALITIES; l < e; ++l) {
    if (locs[l].fatp) {
      network_send(base, locs[l].fatp);
    }
  }

  // 5) Clean up the temporary array.
  free(locs);
}

static int _coalesced_network_send(void *network,  hpx_parcel_t *p) {
  _coalesced_network_t *coalesced_network = network;
  if (!action_is_coalesced(p->action)) {
    return network_send(coalesced_network->base_network, p);
  }

  // Before putting the parcel in the queue, check whether the queue size has
  // reached the  coalescing size then we empty the queue. If that is the case,
  // then try to adjust the parcel count before we proceed to creating fat
  // parcels

  uint64_t parcel_count = sync_load(&coalesced_network->parcel_count,
                                    SYNC_RELAXED);
  while ( parcel_count >= coalesced_network->coalescing_size ) {
    uint64_t readjusted_parcel_count =
    parcel_count - coalesced_network->coalescing_size;
    uint64_t temp_parcel_count = parcel_count;
    sync_fadd(&coalesced_network->syncflush, 1, SYNC_ACQ_REL);
    uint64_t viewed_parcel_count = sync_cas(&coalesced_network->parcel_count,
                                            &temp_parcel_count,
                                            readjusted_parcel_count, SYNC_RELAXED,
                                            SYNC_RELAXED);
    if (viewed_parcel_count == parcel_count) {
      // flush outstanding buffer
      _send_n(coalesced_network, coalesced_network->coalescing_size);
      sync_fadd(&coalesced_network->syncflush, -1, SYNC_ACQ_REL);
      break;
    }
    sync_fadd(&coalesced_network->syncflush, -1, SYNC_ACQ_REL);
    parcel_count = sync_load(&coalesced_network->parcel_count,  SYNC_RELAXED);
  }

  // Prepare the parcel now, 1) to serialize it while its data is probably in
  // our cache and 2) to make sure it gets a pid from the right parent. Put the
  // parcel in the coalesced send queue.
  dbg_assert(p);
  sync_two_lock_queue_enqueue(&coalesced_network->sends, p);
  sync_fadd(&coalesced_network->parcel_count, 1, SYNC_RELAXED);
  return LIBHPX_OK;
}

static int _coalesced_network_progress(void *obj, int id) {
  _coalesced_network_t *network = obj;

  // If the number of buffered parcels is the same as the previous time we
  // progressed, we'll do an eager send operation to reduce latency and make
  // sure we're not inducing deadlock.
  uint64_t current = sync_load(&network->parcel_count, SYNC_RELAXED);
  uint64_t previous = sync_load(&network->previous_parcel_count, SYNC_RELAXED);
  while (current && (current == previous)) {
    // Notify the flush operation that I might be coalescing---this prevents a
    // race during flush where I have taken some parcels out of the queue but
    // not submitted them to the underlying network yet.
    sync_fadd(&network->syncflush, 1, SYNC_ACQ_REL);

    // Try and take all of the current parcels in the coalescing queue.
    if (sync_cas(&network->parcel_count, &current, 0, SYNC_RELAXED,
                 SYNC_RELAXED)) {
      _send_n(network, current);
      current = 0;
    }

    // Notify any flush operations that we're no longer dangerous.
    sync_fadd(&network->syncflush, -1, SYNC_ACQ_REL);

    // Our "previous" is what we just saw (or left behind).
    previous = current;
  }

  // Always post the last value that we saw.
  sync_store(&network->previous_parcel_count, current, SYNC_RELAXED);

  // Call the underlying network progress.
  return network_progress(network->base_network, 0);
}

static int _coalesced_network_command(void *obj, hpx_addr_t locality,
                                      hpx_action_t op, uint64_t args) {
  _coalesced_network_t *coalesced_network = obj;
  return network_command(coalesced_network->base_network, locality, op, args);
}

static int _coalesced_network_pwc(void *obj, hpx_addr_t to, const void *from,
                                  size_t n, hpx_action_t lop, hpx_addr_t laddr,
                                  hpx_action_t rop, hpx_addr_t raddr) {
  _coalesced_network_t *coalesced_network = obj;
  return network_pwc(coalesced_network->base_network, to, from, n, lop, laddr,
                     rop, raddr);
}

static int _coalesced_network_put(void *obj, hpx_addr_t to, const void *from,
                                  size_t n, hpx_action_t lop, hpx_addr_t laddr){
  _coalesced_network_t *coalesced_network = obj;
  return network_put(coalesced_network->base_network, to, from, n, lop, laddr);
}

static int _coalesced_network_get(void *obj, void *to, hpx_addr_t from,
                                  size_t n, hpx_action_t lop, hpx_addr_t laddr)
{
  _coalesced_network_t *coalesced_network = obj;
  return network_get(coalesced_network->base_network, to, from, n, lop, laddr);
}

static hpx_parcel_t* _coalesced_network_probe(void *obj, int rank) {
  _coalesced_network_t *coalesced_network = obj;
  return network_probe(coalesced_network->base_network, rank);
}

static void _coalesced_network_flush(void *obj) {
  _coalesced_network_t *network = obj;

  // coalesce the rest of the buffered sends
  uint64_t count = sync_swap(&network->parcel_count, 0, SYNC_RELAXED);
  if (count > 0) {
    _send_n(network, count);
  }

  // wait for any concurrent flush operations to complete
  while (sync_load(&network->syncflush, SYNC_ACQUIRE))
    /* spin */;

  // and flush the underlying network
  network->base_network->flush(network->base_network);
}

static void _coalesced_network_register_dma(void *obj, const void *base,
                                            size_t bytes, void *key) {
  _coalesced_network_t *coalesced_network = obj;
  network_register_dma(coalesced_network->base_network, base, bytes, key);
}

static void _coalesced_network_release_dma(void *obj, const void *base,
                                           size_t bytes) {
  _coalesced_network_t *coalesced_network = obj;
  network_release_dma(coalesced_network->base_network, base, bytes);
}

static int _coalesced_network_lco_get(void *obj, hpx_addr_t lco, size_t n,
                                      void *out, int reset) {
  _coalesced_network_t *coalesced_network = obj;
  return network_lco_get(coalesced_network->base_network, lco, n, out, reset);
}

static int _coalesced_network_lco_wait(void *obj, hpx_addr_t lco, int reset) {
  _coalesced_network_t *coalesced_network = obj;
  return network_lco_wait(coalesced_network->base_network, lco, reset);
}

network_t* coalesced_network_new (network_t *network,  const struct config *cfg)
{
  _coalesced_network_t *coalesced_network = NULL;
  posix_memalign((void*)&coalesced_network, HPX_CACHELINE_SIZE,
                 sizeof(*coalesced_network));
  if (!coalesced_network) {
    log_error("could not allocate a coalesced network\n");
    return NULL;
  }

  // set the vtable
  coalesced_network->vtable.delete = _coalesced_network_delete;
  coalesced_network->vtable.progress = _coalesced_network_progress;
  coalesced_network->vtable.send =  _coalesced_network_send;
  coalesced_network->vtable.command = _coalesced_network_command;
  coalesced_network->vtable.pwc = _coalesced_network_pwc;
  coalesced_network->vtable.put = _coalesced_network_put;
  coalesced_network->vtable.get = _coalesced_network_get;
  coalesced_network->vtable.probe = _coalesced_network_probe;
  coalesced_network->vtable.flush =  _coalesced_network_flush;
  coalesced_network->vtable.register_dma = _coalesced_network_register_dma;
  coalesced_network->vtable.release_dma = _coalesced_network_release_dma;
  coalesced_network->vtable.lco_get = _coalesced_network_lco_get;
  coalesced_network->vtable.lco_wait = _coalesced_network_lco_wait;

  // set the base network
  coalesced_network->base_network = network;

  // initialize the local coalescing queue for the parcels
  sync_two_lock_queue_init(&coalesced_network->sends, NULL);

  // set coalescing size
  coalesced_network->coalescing_size = cfg->coalescing_buffersize;

  coalesced_network->parcel_count = 0;
  coalesced_network->previous_parcel_count = 0;
  coalesced_network->syncflush = 0;
  log_net("Created coalescing network\n");
  return &coalesced_network->vtable;
}
