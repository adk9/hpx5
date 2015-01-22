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

#include "libhpx/debug.h"
#include "libhpx/libhpx.h"
#include "peer.h"


void peer_fini(peer_t *peer) {
  pwc_buffer_fini(&peer->pwc);
  send_buffer_fini(&peer->send);
  eager_buffer_fini(&peer->tx);
  eager_buffer_fini(&peer->rx);
}


int peer_get(peer_t *peer, void *lva, size_t offset, size_t n, command_t l,
             segid_t segid) {
  segment_t *segment = &peer->segments[segid];
  const void *rva = segment_offset_to_rva(segment, offset);
  struct photon_buffer_priv_t key = segment->key;
  int e = photon_get_with_completion(peer->rank, lva, n, (void*)rva, key, l, 0);
  if (PHOTON_OK != e) {
    return dbg_error("failed transport get operation\n");
  }
  return LIBHPX_OK;
}

int peer_send_rendevous(peer_t *peer, hpx_parcel_t *p, hpx_addr_t lsync) {
  return LIBHPX_EUNIMPLEMENTED;
}
