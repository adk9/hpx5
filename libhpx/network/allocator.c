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
#include <config.h>
#endif

#include <stddef.h>
#include <stdlib.h>

#include "libhpx/debug.h"
#include "libhpx/parcel.h"
#include "libhpx/transport.h"
#include "allocator.h"
#include "cache.h"
#include "padding.h"

struct allocator {
  transport_t *transport;
};


/// thread local parcel cache is lazily allocated
/// TODO: allocate this during initialization
static __thread cache_t *_parcels = NULL;


allocator_t *parcel_allocator_new(transport_t *transport) {
  allocator_t *allocator = malloc(sizeof(*allocator));
  if (!allocator) {
    dbg_error("could not allocate allocator (ironically).\n");
    return NULL;
  }

  allocator->transport = transport;
  return allocator;
}


void parcel_allocator_delete(allocator_t *allocator) {
  free(allocator);
}


hpx_parcel_t *parcel_allocator_get(allocator_t *allocator, int payload) {
  if (!_parcels)
      _parcels = cache_new();

  int size = transport_adjust_size(allocator->transport, payload);

  dbg_log("Getting a parcel of payload size %i, adjusted to %i by the "
          "transport layer.\n", payload, size);

  // try to get a parcel of the right size from the cache, this will deal with
  // misses internally by potentially allocating a new block
  hpx_parcel_t *p = cache_get(_parcels, size);
  if (!p) {
    dbg_error("could not allocate a parcel for %i bytes.\n", size);
    return NULL;
  }

  parcel_init(p, size);
  return p;
}


void parcel_allocator_put(allocator_t *allocator, hpx_parcel_t *p) {
  if (!_parcels)
    _parcels = cache_new();

  parcel_fini(p);
  cache_put(_parcels, p);
}
