// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2016, Trustees of Indiana University,
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

#include "compressed.h"
#include <stdlib.h>
#include <string.h>
#include <libsync/queues.h>
#include <libsync/sync.h>
#include <hpx/hpx.h>
#include <libhpx/action.h>
#include <libhpx/config.h>
#include <libhpx/debug.h>
#include <libhpx/gas.h>
#include <libhpx/libhpx.h>
#include <libhpx/memory.h>
#include <libhpx/network.h>
#include <libhpx/parcel.h>
#include <lz4.h>

// TODO:
// 1. in-place compression and decompression.
// 2. compression and coalescing.

typedef struct {
  Network vtable;
  void *impl;
} _compressed_network_t;

static void _compressed_network_deallocate(void *obj) {
  _compressed_network_t *network = (_compressed_network_t*)obj;
  network_delete(network->impl);
  free(obj);
}

/// Decompress parcels on the receiver side.
///
/// @param       buffer The compressed parcel.
/// @param            n The size of @p buffer.
static int _decompress_handler(char* buffer, int n) {
  // retrieve the original size from the payload.
  size_t size = *(size_t*)buffer;
  hpx_parcel_t *p = (hpx_parcel_t *)as_memalign(AS_REGISTERED, HPX_CACHELINE_SIZE, size);
  buffer += sizeof(size_t);
  int osize = LZ4_decompress_fast(buffer, (char*)p, size);
  dbg_assert(osize == n);

  p->ustack = NULL;
  p->next = NULL;
  parcel_set_state(p, PARCEL_SERIALIZED);
  parcel_launch(p);
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _decompress, _decompress_handler,
                     HPX_POINTER, HPX_INT);

static int _compressed_network_send(void *obj, hpx_parcel_t *p,
                                    hpx_parcel_t *ssync) {
  _compressed_network_t *network = (_compressed_network_t*)obj;
  if (!action_is_compressed(p->action)) {
    return network_send(network->impl, p, ssync);
  }

  // allocate a new enclosing parcel
  size_t isize = parcel_size(p);
  size_t bytes = LZ4_compressBound(isize) + sizeof(size_t);

  hpx_parcel_t *cp = parcel_new(p->target, _decompress, 0, 0,
                                p->pid, 0, bytes);
  dbg_assert(cp);

  // the original size is stored in the payload since we need it
  // during decompression.
  char *data = (char*)hpx_parcel_get_data(cp);
  *(size_t*)data = isize;
  data += sizeof(size_t);

  // compress the original parcel
  size_t osize = LZ4_compress_fast((const char*)p, data, isize, bytes, 1);
  // if compression fails, send the original parcel
  if (!osize) {
    parcel_delete(cp);
    cp = p;
  } else {
    parcel_delete(p);
    cp->size = osize + sizeof(size_t);
  }
  return network_send(network->impl, cp, ssync);
}

static int _compressed_network_progress(void *obj, int id) {
  _compressed_network_t *network = (_compressed_network_t*)obj;
  return network_progress(network->impl, id);
}

static hpx_parcel_t* _compressed_network_probe(void *obj, int rank) {
  _compressed_network_t *network = (_compressed_network_t*)obj;
  return network_probe(network->impl, rank);
}

static void _compressed_network_flush(void *obj) {
  _compressed_network_t *network = (_compressed_network_t*)obj;
  network_flush(network->impl);
}

static void _compressed_network_register_dma(void *obj, const void *base,
                                            size_t bytes, void *key) {
  _compressed_network_t *network = (_compressed_network_t*)obj;
  network_register_dma(network->impl, base, bytes, key);
}

static void _compressed_network_release_dma(void *obj, const void *base,
                                           size_t bytes) {
  _compressed_network_t *network = (_compressed_network_t*)obj;
  network_release_dma(network->impl, base, bytes);
}

static int _compressed_network_lco_wait(void *obj, hpx_addr_t lco, int reset) {
  _compressed_network_t *network = (_compressed_network_t*)obj;
  return network_lco_wait(network->impl, lco, reset);
}

static int _compressed_network_lco_get(void *obj, hpx_addr_t lco, size_t n,
                                      void *out, int reset) {
  _compressed_network_t *network = (_compressed_network_t*)obj;
  return network_lco_get(network->impl, lco, n, out, reset);
}

void* compressed_network_new(void *impl) {
  dbg_assert(impl);
  _compressed_network_t *network = (_compressed_network_t*)malloc(sizeof(*network));

  network->vtable.string       = ((Network*)impl)->string;
  network->vtable.type         = ((Network*)impl)->type;
  network->vtable.deallocate   = _compressed_network_deallocate;
  network->vtable.progress     = _compressed_network_progress;
  network->vtable.send         = _compressed_network_send;
  network->vtable.probe        = _compressed_network_probe;
  network->vtable.flush        = _compressed_network_flush;
  network->vtable.register_dma = _compressed_network_register_dma;
  network->vtable.release_dma  = _compressed_network_release_dma;
  network->vtable.lco_get      = _compressed_network_lco_get;
  network->vtable.lco_wait     = _compressed_network_lco_wait;
  network->vtable.coll_init    = ((Network*)impl)->coll_init;
  network->vtable.coll_sync    = ((Network*)impl)->coll_sync;

  network->impl = impl;

  log_net("Enabled parcel compression.\n");
  return &network->vtable;
}
