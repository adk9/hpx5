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
#include "eager_buffer.h"
#include "peer.h"
#include "pwc.h"
#include "send_buffer.h"
#include "../../gas/pgas/gpa.h"                 // sort of a hack

/// The record type for the pending send circular buffer.
typedef struct {
  hpx_parcel_t  *p;
  hpx_addr_t lsync;
} record_t;


static hpx_action_t _finish_get_rx_min;

/// Initiate an rdma get operation for the send buffer.
static int _start_get_rx_min(send_buffer_t *sends) {
  const size_t minoffset = offsetof(peer_t, rx) + offsetof(eager_buffer_t, min);
  size_t offset = here->rank * sizeof(peer_t) + minoffset;
  // compute the offset into the peer segment
  peer_t *p = sends->tx->peer;
  uint64_t *min = &sends->tx->min;
  command_t cmd = encode_command(_finish_get_rx_min, p->rank);
  int status = peer_get(p, min, offset, sizeof(*min), cmd, SEGMENT_PEERS);
  if (status != LIBHPX_OK) {
    dbg_error("could not initiate get with transport\n");
  }
  return status;
}

/// Append a record to the parcel's pending send buffer.
///
/// @param        sends The send buffer.
/// @param            p The parcel to buffer.
/// @param        lsync The local command.
///
/// @returns  LIBHXP_OK The parcel was buffered successfully.
///        LIBHPX_ERROR A pending record could not be allocated.
static int _append(send_buffer_t *sends, hpx_parcel_t *p, hpx_addr_t lsync) {
  record_t *r = circular_buffer_append(&sends->pending);
  dbg_assert_str(r, "could not append a send operation to the buffer\n");
  r->p = p;
  r->lsync = lsync;
  return LIBHPX_OK;
}

/// Wrap the eager_buffer_tx() operation in an interface that matches the
/// circular_buffer_progress callback type.
static int _start_record(void *buffer, void *record) {
  send_buffer_t *sends = buffer;
  record_t *r = record;
  return eager_buffer_tx(sends->tx, r->p);
}

/// Progress a send buffer.
///
/// Progressing a send buffer means transferring as many buffered sends to the
/// network as is currently possible. This will return the number of remaining
/// buffered sends.
///
/// Progressing a send buffer must be properly synchronized with the send
/// operation, as well as with concurrent attempts to progress the buffer, since
/// they may be called concurrently from more than one thread.
///
/// @param        sends The send buffer.
///
/// @returns            HPX_SUCCESS or an error code.
static int _send_buffer_progress(send_buffer_t *sends) {
  int status = HPX_SUCCESS;
  sync_tatas_acquire(&sends->lock);
  int i = circular_buffer_progress(&sends->pending, _start_record, sends);
  if (i < 0) {
    log_error("failed to progress the send buffer\n");
    status = HPX_ERROR;
  }

  // If there are still sends remaining, then regenerate the rdma get operation
  // to read the remote min. This will trigger another instance of this progress
  // loop when that get completes.
  if (i > 0) {
    if (_start_get_rx_min(sends) != LIBHPX_OK) {
      log_error("error initiating an rdma get operation\n");
      status = HPX_ERROR;
    }
  }
  sync_tatas_release(&sends->lock);
  return status;
}

/// This handler is run when the get_rx_min operation completes.
///
/// The handler uses the target data to encode the peer for which the rDMA
/// occurred. This signal indicates that we have an opportunity to progress the
/// peer's eager send buffer.
static HPX_ACTION(_finish_get_rx_min, void *UNUSED) {
  uint32_t id = pgas_gpa_to_offset(hpx_thread_current_target());
  peer_t *peer = pwc_get_peer(here->network, id);
  dbg_assert_str(peer, "invalid peer id %u\n", id);
  log_net("updated min to %lu\n", peer->tx.min);
  return _send_buffer_progress(&peer->send);
}

int send_buffer_init(send_buffer_t *sends, struct eager_buffer *tx,
                     uint32_t size) {
  sync_tatas_init(&sends->lock);
  sends->tx = tx;
  return circular_buffer_init(&sends->pending, sizeof(record_t), size);
}

void send_buffer_fini(send_buffer_t *sends) {
  circular_buffer_fini(&sends->pending);
}

int send_buffer_send(send_buffer_t *sends, hpx_parcel_t *p, hpx_addr_t lsync) {
  if (lsync != HPX_NULL) {
    log_error("local send complete event unimplemented\n");
    return LIBHPX_EUNIMPLEMENTED;
  }

  int status = LIBHPX_OK;
  sync_tatas_acquire(&sends->lock);

  // If we have no pending sends, try and start a request.
  if (circular_buffer_size(&sends->pending) == 0) {
    status = eager_buffer_tx(sends->tx, p);
    if (status == LIBHPX_OK) {
      goto unlock;
    }

    // If it the eager buffer tells us to retry, then we start an rmda request
    // to read the remote rx progress.
    if (status == LIBHPX_RETRY) {
      status = _start_get_rx_min(sends);
    }

    // If we have an error at this point then report it and buffer the parcel.
    if (status != LIBHPX_OK) {
      log_error("error in parcel send, buffer the operation\n");
    }
  }

  // We need to buffer this parcel, because either we're already buffering
  // parcels, or we need to buffer while the rdma get occurs.
  status = _append(sends, p, lsync);
  dbg_check(status, "could not append send operation\n");

 unlock:
  sync_tatas_release(&sends->lock);
  return status;
}
