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
#include "libhpx/locality.h"
#include "libhpx/parcel.h"
#include "peer.h"
#include "pwc.h"

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

typedef struct {
  size_t bytes;
  hpx_addr_t addr;
} _get_parcel_args_t;


static HPX_DEFDECL_ACTION(PINNED_ACTION, _free_parcel, void *UNUSED) {
  hpx_parcel_t *p = hpx_thread_current_local_target();
  assert(p);
  hpx_parcel_release(p);
  return HPX_SUCCESS;
}

static HPX_ACTION(_get_parcel, void *args) {
  // Extract arguments.
  _get_parcel_args_t *a = args;
  size_t bytes = a->bytes;
  hpx_addr_t addr = a->addr;

  // Allocate a parcel to receive in.
  hpx_parcel_t *p = hpx_parcel_acquire(NULL, bytes);
  if (!p) {
    dbg_error("Could not allocate a parcel for rendevous protocol\n");
    return HPX_ERROR;
  }

  // Find the peer and set the parcel's src.
  p->src = gas_owner_of(here->gas, addr);
  peer_t *peer = pwc_get_peer(here->network, p->src);

  // get the local destination, size, and remote offset for an rdma transfer
  void *to = parcel_network_offset(p);
  uint32_t n = parcel_network_size(p);
  hpx_addr_t paddr = hpx_addr_add(addr, parcel_prefix_size(), UINT32_MAX);
  size_t offset = gas_offset_of(here->gas, paddr);

  // create a local command that we can wait for
  hpx_addr_t lsync = hpx_lco_future_new(0);
  command_t cmp = encode_command(hpx_lco_set_action, lsync);

  // perform the get operation
  int status = HPX_SUCCESS;
  if (LIBHPX_OK != peer_get(peer, to, offset, n, cmp, SEGMENT_HEAP)) {
    status = HPX_ERROR;
    goto unwind;
  }

  // wait until it completes
  if (LIBHPX_OK != hpx_lco_wait(lsync)) {
    status = HPX_ERROR;
    goto unwind;
  }

  // launch the parcel
  parcel_launch(p);
  hpx_lco_delete(lsync, HPX_NULL);
  hpx_call_cc(addr, _free_parcel, NULL, 0, NULL, NULL);

 unwind:
  hpx_parcel_release(p);
  hpx_lco_delete(lsync, HPX_NULL);
  return status;
}

int peer_send_rendevous(peer_t *peer, hpx_parcel_t *p, hpx_addr_t lsync) {
  _get_parcel_args_t args = {
    .bytes = parcel_size(p),
    .addr = lva_to_gva(p)
  };
  assert(args.addr != HPX_NULL);
  return hpx_call(HPX_THERE(peer->rank), _get_parcel, lsync, &args,
                  sizeof(args));
}
