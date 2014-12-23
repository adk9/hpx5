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
# include "config.h"
#endif

#include <stddef.h>
#include "libhpx/debug.h"
#include "libhpx/libhpx.h"
#include "libhpx/parcel.h"
#include "eager_buffer.h"
#include "peer.h"

static uint32_t _index_of(eager_buffer_t *buffer, uint64_t i) {
  return (i & (buffer->size - 1));
}

int eager_buffer_init(eager_buffer_t* b, peer_t *peer, char *base,
                      uint32_t size)
{
  b->peer = peer;
  b->size = size;
  b->min = 0;
  b->max = 0;
  b->base = base;
  return LIBHPX_OK;
}

void eager_buffer_fini(eager_buffer_t *b) {
}

int eager_buffer_tx(eager_buffer_t *buffer, hpx_parcel_t *p, hpx_addr_t lsync) {
  uint32_t n = parcel_network_size(p);
  if (n > buffer->size) {
    return dbg_error("cannot send %u byte parcels via eager put\n", n);
  }

  uint64_t end = buffer->max + n;
  if (end - buffer->min > buffer->size) {
    dbg_log("could not send %u byte parcel\n", n);
    return LIBHPX_RETRY;
  }

  uint32_t roff = _index_of(buffer, buffer->max);
  uint32_t eoff = _index_of(buffer, end);
  int wrapped = (eoff <= roff);
  if (wrapped) {
    dbg_log("can not handle wrapped parcel buffer\n");
    return LIBHPX_ERROR;
  }

  buffer->max += n;

  const void *lva = parcel_network_offset(p);
  uint64_t completion = 0;
  return peer_pwc(buffer->peer, roff, lva, n, lsync, HPX_NULL, completion,
                  SEGMENT_EAGER);
}

hpx_parcel_t *eager_buffer_rx(eager_buffer_t *buffer) {
  return NULL;
}
