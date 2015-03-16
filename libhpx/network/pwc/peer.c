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

#include "libhpx/debug.h"
#include "libhpx/libhpx.h"
#include "libhpx/locality.h"
#include "libhpx/parcel.h"
#include "commands.h"
#include "parcel_utils.h"
#include "peer.h"
#include "pwc.h"
#include "xport.h"
#include "../../gas/pgas/gpa.h"
#include "../../gas/pgas/pgas.h"

void peer_fini(peer_t *peer) {
  send_buffer_fini(&peer->send);
  eager_buffer_fini(&peer->tx);
  eager_buffer_fini(&peer->rx);
}

int peer_pwc(xport_op_t *op, peer_t *peer, size_t roff, segid_t segment_id) {
  segment_t *segment = &peer->segments[segment_id];
  op->dest = segment_offset_to_rva(segment, roff);
  op->dest_key =segment->key;
  return peer->xport->pwc(op);
}

int peer_put_command(peer_t *p, command_t rsync) {
  xport_op_t op = {
    .rank = p->rank,
    .flags = 0,
    .n = 0,
    .dest = NULL,
    .dest_key = NULL,
    .src = NULL,
    .src_key = NULL,
    .lop = 0,
    .rop = rsync
  };
  return peer_pwc(&op, p, 0, SEGMENT_NULL);
}

int peer_send(peer_t *peer, hpx_parcel_t *p, hpx_addr_t lsync) {
  return send_buffer_send(&peer->send, p, lsync);
}

int peer_get(peer_t *peer, void *lva, size_t offset, size_t n, command_t lsync,
             segid_t segid) {
  segment_t *segment = &peer->segments[segid];
  xport_op_t op = {
    .rank = peer->rank,
    .flags = 0,
    .n = n,
    .dest = lva,
    .dest_key = NULL,
    .src = segment_offset_to_rva(segment, offset),
    .src_key = segment->key,
    .lop = lsync,
    .rop = 0
  };
  return peer->xport->gwc(&op);
}

/// This local action just wraps the hpx_lco_set operation in an action that can
/// be used as a network operation.
static int _lco_set_handler(int src, command_t command) {
  uint64_t offset = command_get_arg(command);
  hpx_addr_t lco = pgas_offset_to_gpa(here->rank, offset);
  hpx_lco_set(lco, 0, NULL, HPX_NULL, HPX_NULL);
  return HPX_SUCCESS;
}
static HPX_ACTION_DEF(INTERRUPT, _lco_set_handler, _lco_set, HPX_INT,
                      HPX_UINT64);

/// Perform a rendezvous get operation on a parcel.
///
/// The source of a parcel send will generate this event at the target, in order
/// to get the target to RDMA-get a large parcel.
static int _get_parcel_handler(size_t bytes, hpx_addr_t from) {
  // figure out where the parcel is coming from.
  int src = gas_owner_of(here->gas, from);

  // Allocate a parcel to receive in.
  hpx_parcel_t *p = hpx_parcel_acquire(NULL, bytes);
  if (!p) {
    log_error("Could not allocate a parcel for rendezvous protocol\n");
    return HPX_ERROR;
  }

  // initialize the parcel's source
  p->src = src;

  // use our peer to src in order to figure out the arguments we need to do a
  // memget(p, from, bytes).
  peer_t *peer = pwc_get_peer(src);
  void *to = pwc_network_offset(p);
  uint32_t n = pwc_network_size(p);
  hpx_addr_t addr = hpx_addr_add(from, pwc_prefix_size(), UINT32_MAX);
  size_t offset = gas_offset_of(here->gas, addr);

  // Create a future and a command to run when this rdma completes.
  hpx_addr_t lsync = hpx_lco_future_new(0);
  command_t cmp = encode_command(_lco_set, lsync);

  if (LIBHPX_OK != peer_get(peer, to, offset, n, cmp, SEGMENT_HEAP) ||
      LIBHPX_OK != hpx_lco_wait(lsync)) {
    hpx_parcel_release(p);
    hpx_lco_delete(lsync, HPX_NULL);
    return HPX_ERROR;
  } else {
    hpx_lco_delete(lsync, HPX_NULL);
    parcel_launch(p);
    hpx_call_cc(from, free_parcel, NULL, NULL, &here->rank, &from);
  }
}
static HPX_ACTION_DEF(DEFAULT, _get_parcel_handler, _get_parcel, HPX_SIZE_T,
                      HPX_ADDR);

int peer_send_rendezvous(peer_t *peer, hpx_parcel_t *p, hpx_addr_t lsync) {
  size_t bytes = parcel_size(p); // this type must match the type of
                                 // _get_parcel'a first argument
  hpx_addr_t gva = lva_to_gva(p);
  return hpx_xcall(HPX_THERE(peer->rank), _get_parcel, lsync, bytes, gva);
}
