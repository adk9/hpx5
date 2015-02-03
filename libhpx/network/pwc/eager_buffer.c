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
#include "commands.h"
#include "eager_buffer.h"
#include "peer.h"
#include "pwc.h"
#include "../../gas/pgas/gpa.h"                 // sort of a hack

static uint32_t _index_of(eager_buffer_t *buffer, uint64_t i) {
  return (i & (buffer->size - 1));
}

/// Command sent to receive in the eager buffer.
static HPX_INTERRUPT(_eager_rx, int *src) {
  peer_t *peer = pwc_get_peer(here->network, *src);
  eager_buffer_t *eager = &peer->rx;
  hpx_parcel_t *parcel = eager_buffer_rx(eager);
  log_net("received eager parcel bytes from %d (%s)\n", *src,
          action_table_get_key(here->actions, parcel->action));
  scheduler_spawn(parcel);
  return HPX_SUCCESS;
}

/// Finish a local eager parcel send operation.
///
/// This is run as a local completion command. We'd prefer this to be an
/// interrupt, but the current framework doesn't have any way to pass the parcel
/// address through into the handler, it has to come out of band through
/// hpx_thread_current_target(). Interrupts don't have access to this
/// out-of-band data, so a task it is.
///
/// This is okay, but is likely to be higher overhead than we really want. We
/// could consider changing the type of network commands...
static HPX_TASK(_finish_eager_tx, void *UNUSED) {
  hpx_parcel_t *p = NULL;
  hpx_addr_t target = hpx_thread_current_target();
  if (!hpx_gas_try_pin(target, (void**)&p)) {
    return dbg_error("could not finish eager tx\n");
  }
  log_net("releasing sent parcel\n");
  hpx_parcel_release(p);
  hpx_gas_unpin(target);
  return HPX_SUCCESS;
}

/// This handles the wrap operation at the receiver.
///
/// This currently assumes that rx buffer operations occur in-order, so that
/// when the wrap commands arrives we can just increment the min index the
/// amount remaining to exactly wrap the buffer.
static HPX_INTERRUPT(_eager_rx_wrap, int *src) {
  peer_t *peer = pwc_get_peer(here->network, *src);
  eager_buffer_t *rx = &peer->rx;
  uint32_t r = rx->size - _index_of(rx, rx->min);
  rx->min += r;
  dbg_assert_str(_index_of(rx, rx->min) == 0,
                 "%u bytes did not unwrap the buffer\n", r);
  log_net("eager buffer %d wrapped (%u bytes)\n", *src, r);
  return HPX_SUCCESS;
}

/// A utility function to inject padding into an eager buffer.
///
/// This is used when we get a send that will wrap around the buffer. It is
/// implemented using a single command---this is important because we don't use
/// any eager buffer bytes.
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
static int _wrap(eager_buffer_t *tx, hpx_parcel_t *p, uint32_t bytes) {
  log_net("sending %u bytes of padding\n", bytes);
  command_t cmd = encode_command(_eager_rx_wrap, 0);
  int status = peer_put_command(tx->peer, cmd);
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
    log_net("%u byte parcel may overflow buffer\n", n);
    return LIBHPX_RETRY;
  }

  const uint32_t roff = _index_of(tx, tx->max);
  const uint32_t eoff = _index_of(tx, end);
  if (eoff <= roff) {
    return _wrap(tx, p, tx->size - roff);
  }

  log_net("sending %d byte parcel to %d (%s)\n",
          n, tx->peer->rank, action_table_get_key(here->actions, p->action));

  int e = peer_pwc(tx->peer,                     // peer structure
                   tx->base + roff,              // remote offset
                   parcel_network_offset(p),     // local address
                   n,                            // # bytes
                   encode_command(_finish_eager_tx, lva_to_gva(p)), // local completion
                   HPX_NULL,                     // remote completion
                   encode_command(_eager_rx, 0), // remote command
                   SEGMENT_EAGER                 // segment
                  );

  if (e == LIBHPX_OK) {
    tx->max += n;
  }
  return e;
}

hpx_parcel_t *eager_buffer_rx(eager_buffer_t *rx) {
  // Figure out how much data we're going to copy, then allocate a parcel to
  // copy out the data to. Then perform the copy and return the parcel.
  //
  // NB: We technically want to process from the buffer directly.
  const uint32_t i = _index_of(rx, rx->min);
  const uint64_t offset = rx->base + i;
  const void *from = rx->peer->segments[SEGMENT_EAGER].base + offset;
  const uint32_t bytes = *(const uint32_t *)from;

  hpx_parcel_t *p = hpx_parcel_acquire(NULL, bytes);
  dbg_assert_str(p != NULL,"failed to allocate a parcel in eager receive\n");
  memcpy(parcel_network_offset(p), from, parcel_network_size(p));

  // update the progress in this buffer
  rx->min += parcel_network_size(p);

  // Make sure the parcel came from where we think it came from
  dbg_assert(p->src == rx->peer->rank);
  return p;
}
