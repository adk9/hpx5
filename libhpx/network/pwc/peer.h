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
#ifndef LIBHPX_NETWORK_PWC_PEER_H
#define LIBHPX_NETWORK_PWC_PEER_H

#include "segment.h"
#include "pwc_buffer.h"

struct gas;

typedef enum {
  SEGMENT_HEAP,
  SEGMENT_PARCEL,
  SEGMENT_MAX
} segment_id_t;


typedef struct {
  uint32_t         rank;
  const uint32_t UNUSED;
  segment_t    segments[SEGMENT_MAX];
  pwc_buffer_t     puts;
} peer_t;

static inline int peer_put(peer_t *peer, size_t roff, const void *lva, size_t n,
                           hpx_addr_t local, hpx_addr_t remote, hpx_action_t op,
                           segment_id_t segment_id) {
  pwc_buffer_t *puts = &peer->puts;
  segment_t *segment = &peer->segments[segment_id];
  return pwc_buffer_put(puts, roff, lva, n, local, remote, op, segment);
}


int peer_get(peer_t *peer, void *lva, size_t offset, size_t n, hpx_addr_t local)
  HPX_INTERNAL HPX_NON_NULL(1);

#endif
