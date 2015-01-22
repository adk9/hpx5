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

#include "libhpx/action.h"
#include "libhpx/debug.h"
#include "libhpx/libhpx.h"
#include "libhpx/locality.h"
#include "libhpx/parcel.h"
#include "libhpx/scheduler.h"
#include "completions.h"
#include "eager_buffer.h"
#include "peer.h"
#include "pwc.h"
#include "../../gas/pgas/gpa.h"                 // sort of a hack

static uint32_t _index_of(eager_buffer_t *buffer, uint64_t i) {
  return (i & (buffer->size - 1));
}

static HPX_INTERRUPT(_eager_rx)(void *args) {
  int *src = args;
  uint32_t bytes = pgas_gpa_to_offset(hpx_thread_current_target());
  peer_t *peer = pwc_get_peer(here->network, *src);
  eager_buffer_t *eager = &peer->rx;
  hpx_parcel_t *parcel = eager_buffer_rx(eager, bytes);
  dbg_log_net("received %u eager parcel bytes from %d (%s)\n", bytes, *src,
              action_table_get_key(here->actions, parcel->action));
  scheduler_spawn(parcel);
  return HPX_SUCCESS;
}

static HPX_INTERRUPT(_finish_eager_tx)(void *UNUSED) {
  hpx_parcel_t *p = NULL;
  hpx_addr_t target = hpx_thread_current_target();
  if (!hpx_gas_try_pin(target, (void**)&p)) {
    return dbg_error("could not finish eager tx\n");
  }
  dbg_log_net("releasing sent parcel\n");
  hpx_parcel_release(p);
  hpx_gas_unpin(target);
  return HPX_SUCCESS;
}

/// This handles the pad operation at the receiver.
///
/// This currently assumes that the only purpose for padding is to deal with a
/// wrapped buffer, so it contains a check to see if the number of bytes are in
/// agreement with what we think we need to unwrap the buffer.
static HPX_INTERRUPT(_eager_rx_pad)(void *args) {
  int *src = args;
  uint32_t bytes = pgas_gpa_to_offset(hpx_thread_current_target());
  peer_t *peer = pwc_get_peer(here->network, *src);
  eager_buffer_t *rx = &peer->rx;
  rx->min += bytes;
  DEBUG_IF (_index_of(rx, rx->min) != 0) {
    dbg_error("%u bytes did not unwrap the buffer\n", bytes);
  }
  dbg_log_net("received %u bytes of padding from %u\n", bytes, *src);
  return HPX_SUCCESS;
}

/// A utility function to inject padding into an eager buffer.
///
/// This is used when we get a send that will wrap around the buffer. It is
/// implemented using a single command---this is important because we don't use
/// any eager buffer bytes and thus don't care what the value of bytes is.
///
/// If the buffer is being processed in-order, which we expect, then the @p
/// bytes does not need to be encoded in the command, since the tx/rx indexes
/// are in agreement, the receiver can compute the padding too. We do send the
/// bytes though, since it doesn't use any extra bandwidth and allows us to add
/// padding for reasons other than wrapped buffers.
///
/// This function will recursively send the parcel.
///
/// @param       buffer The tx buffer to pad.
/// @param            p The parcel that triggered the padding.
/// @param        bytes The size of the padding.
///
/// @returns  LIBHPX_OK The result of the parcel send operation if the padding
///                       was injected correctly.
///
static int _pad(eager_buffer_t *tx, hpx_parcel_t *p, uint32_t bytes) {
  dbg_log_net("sending %u bytes of padding\n", bytes);
  completion_t op = encode_completion(_eager_rx_pad, (uint64_t)bytes);
  int status = peer_put_control(tx->peer, op);
  if (status != LIBHPX_OK) {
    return dbg_error("could not send command to pad eager buffer\n");
  }
  else {
    tx->max += bytes;
    return eager_buffer_tx(tx, p);
  }
}

int eager_buffer_init(eager_buffer_t* b, peer_t *p, uint64_t base,
                      uint32_t size) {
  b->peer = p;
  b->size = size;
  b->min = 0;
  b->max = 0;
  b->base = base;
  return LIBHPX_OK;
}

void eager_buffer_fini(eager_buffer_t *b) {
}

int eager_buffer_tx(eager_buffer_t *tx, hpx_parcel_t *p) {
  const uint32_t n = parcel_network_size(p);
  if (n > tx->size) {
    return dbg_error("cannot send %u bytes via eager parcel buffer\n", n);
  }

  const uint64_t end = tx->max + n;
  if (end > (1ull << GPA_OFFSET_BITS)) {
    dbg_error("lifetime send buffer overflow handling unimplemented\n");
    return LIBHPX_EUNIMPLEMENTED;
  }

  if (end - tx->min > tx->size) {
    dbg_log_net("%u byte parcel may overflow buffer\n", n);
    return LIBHPX_RETRY;
  }

  const uint32_t roff = _index_of(tx, tx->max);
  const uint32_t eoff = _index_of(tx, end);
  if (eoff <= roff) {
    return _pad(tx, p, tx->size - roff);
  }

  tx->max += n;

  const void *lva = parcel_network_offset(p);
  const hpx_addr_t parcel = lva_to_gva(p);
  assert(parcel != HPX_NULL);
  const completion_t lsync = encode_completion(_finish_eager_tx, parcel);
  const completion_t completion = encode_completion(_eager_rx, n);
  dbg_log_net("sending %d byte parcel to %d (%s)\n", n, tx->peer->rank,
              action_table_get_key(here->actions, p->action));
  return peer_pwc(tx->peer, roff + tx->base, lva, n, lsync, HPX_NULL,
                  completion, SEGMENT_EAGER);
}

hpx_parcel_t *eager_buffer_rx(eager_buffer_t *rx, uint32_t bytes) {
  // allocate a parcel to copy out to
  const uint32_t size = bytes - parcel_prefix_size();
  hpx_parcel_t *p = hpx_parcel_acquire(NULL, size);
  DEBUG_IF(!p) {
    dbg_error("failed to allocate a parcel in eager receive\n");
  }

  // copy the parcel data
  const uint32_t i = _index_of(rx, rx->min);
  const uint64_t offset = rx->base + i;
  const void *from = rx->peer->segments[SEGMENT_EAGER].base + offset;
  memcpy(parcel_network_offset(p), from, bytes);

  // update the progress in this buffer
  rx->min += bytes;

  // fill in the parcel source data and return
  p->src = rx->peer->rank;
  return p;
}
