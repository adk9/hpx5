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

#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libhpx/instrumentation.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/parcel.h>
#include <libhpx/scheduler.h>
#include "commands.h"
#include "eager_buffer.h"
#include "parcel_utils.h"
#include "peer.h"
#include "pwc.h"
#include "xport.h"
#include "../../gas/pgas/pgas.h"                 // sort of a hack

static uint32_t _index_of(eager_buffer_t *buffer, uint64_t i) {
  return (i & (buffer->size - 1));
}

/// Command sent to receive in the eager buffer.
static int _eager_rx_handler(int src, uint64_t command) {
  peer_t *peer = pwc_get_peer(src);
  eager_buffer_t *eager = &peer->rx;
  hpx_parcel_t *parcel = eager_buffer_rx(eager);
  log_net("received eager parcel bytes from %d (%s)\n", src,
          action_table_get_key(here->actions, parcel->action));
  scheduler_spawn(parcel);
  return HPX_SUCCESS;
}
static HPX_ACTION_DEF(INTERRUPT, _eager_rx_handler, _eager_rx, HPX_INT,
                      HPX_UINT64);

/// Free a parcel.
static int _free_parcel_handler(int src, command_t command) {
  uint64_t offset = command_get_arg(command);
  hpx_parcel_t *p = (void*)(uintptr_t)offset;
  log_net("releasing sent parcel %p\n", (void*)p);
  hpx_parcel_release(p);
  return HPX_SUCCESS;
}
HPX_ACTION_DEF(INTERRUPT, _free_parcel_handler, free_parcel, HPX_INT,
               HPX_UINT64);

/// This handles the wrap operation at the receiver.
///
/// This currently assumes that rx buffer operations occur in-order, so that
/// when the wrap commands arrives we can just increment the min index the
/// amount remaining to exactly wrap the buffer.
static int _eager_rx_wrap_handler(int src, command_t command) {
  peer_t *peer = pwc_get_peer(src);
  eager_buffer_t *rx = &peer->rx;
  uint32_t min = _index_of(rx, rx->min);
  uint32_t r = (rx->size - min);
  dbg_assert(r != 0);
  rx->min += r;
  dbg_assert_str(_index_of(rx, rx->min) == 0,
                 "%u bytes did not unwrap the buffer\n", r);
  log_net("eager buffer %d wrapped (%u bytes)\n", src, r);
  return HPX_SUCCESS;
}
static HPX_ACTION_DEF(INTERRUPT, _eager_rx_wrap_handler, _eager_rx_wrap,
                      HPX_INT, HPX_UINT64);

static int _buffer_tx(eager_buffer_t *tx, hpx_parcel_t *p);

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
static int _wrap(eager_buffer_t *tx, hpx_parcel_t *p, uint32_t bytes) {
  dbg_assert(bytes != 0);
  int target = tx->peer->rank;
  log_net("wrapping rank %d eager buffer (%u bytes) at sequence # %lu\n",
          target, bytes, tx->sequence);
  command_t rsync = command_pack(_eager_rx_wrap, HPX_THERE(target));
  int status = peer_put_command(tx->peer, rsync);
  dbg_check(status, "could not send command to pad eager buffer\n");
  tx->max += bytes;
  return _buffer_tx(tx, p);
}

static int _buffer_tx(eager_buffer_t *tx, hpx_parcel_t *p) {
  const uint32_t n = pwc_network_size(p);
  if (n > tx->size) {
    return log_error("cannot send %u bytes via eager parcel buffer\n", n);
  }

  const uint64_t end = tx->max + n;
  if (end > pgas_max_offset()) {
    log_error("lifetime send buffer overflow handling unimplemented\n");
    return LIBHPX_EUNIMPLEMENTED;
  }

  if (end - tx->min > tx->size) {
    log_net("%u byte parcel may overflow buffer (think min is %lu)\n", n,
            tx->min);
    return LIBHPX_RETRY;
  }

  const uint32_t roff = _index_of(tx, tx->max);
  const uint32_t eoff = _index_of(tx, end);
  if (eoff <= roff) {
    return _wrap(tx, p, tx->size - roff);
  }

  uint64_t sequence = tx->sequence++;
#ifdef ENABLE_DEBUG
  p->sequence = sequence;
#endif

  static const int class = HPX_INST_CLASS_NETWORK_PWC;
  static const int id = HPX_INST_EVENT_NETWORK_PWC_SEND;
  void *rva = tx->peer->segments[SEGMENT_EAGER].base + tx->tx_base + roff;
  inst_trace(class, id, sequence, n, (uint64_t)rva, tx->peer->rank);

  const void *base = pwc_network_offset(p);
  peer_t *peer = tx->peer;
  xport_op_t op = {
    .rank = peer->rank,
    .n = n,
    .dest = NULL,
    .dest_key = NULL,
    .src = base,
    .src_key = NULL,
    .lop = command_pack(free_parcel, (uint64_t)(uintptr_t)p),
    .rop = command_pack(_eager_rx, HPX_THERE(peer->rank))
  };

  int e = peer_pwc(&op, tx->peer, tx->tx_base + roff, SEGMENT_EAGER);

  if (e == LIBHPX_OK) {
    tx->max += n;
  }
  else {
    log_error("failed pwc to peer %d\n", peer->rank);
  }
  return e;

  // suppress warnings
  (void)rva;
  (void)sequence;
}

int eager_buffer_init(eager_buffer_t* b, peer_t *p, uint64_t tx_base,
                      char *rx_base, uint32_t size) {
  b->peer = p;
  // sync_tatas_init(&b->lock);
  b->size = size;
  b->sequence = 0;
  b->min = 0;
  b->max = 0;
  b->tx_base = tx_base;
  b->rx_base = rx_base;
  return LIBHPX_OK;
}

void eager_buffer_fini(eager_buffer_t *b) {
}

int eager_buffer_tx(eager_buffer_t *tx, hpx_parcel_t *p) {
  int status = LIBHPX_OK;
  // sync_tatas_acquire(&tx->lock);
  status = _buffer_tx(tx, p);
  // sync_tatas_release(&tx->lock);
  return status;
}

hpx_parcel_t *eager_buffer_rx(eager_buffer_t *rx) {
  // Figure out how much data we're going to copy, then allocate a parcel to
  // copy out the data to. Then perform the copy and return the parcel.
  //
  // NB: We technically want to process from the buffer directly.
  const uint32_t i = _index_of(rx, rx->min);
  dbg_assert_str(i + sizeof(uint32_t) < rx->size,
                 "buffer should have wrapped\n");
  dbg_assert_str(rx->rx_base, "cannot receive from a tx buffer\n");
  const void *from = rx->rx_base + i;
  uint32_t size = 0;
  memcpy(&size, from, sizeof(size));            // strict-aliasing
  if (DEBUG) {
    hpx_parcel_t *in = (hpx_parcel_t*)((char*)from - pwc_prefix_size());
    dbg_assert(in->size == size);
    dbg_assert(hpx_gas_try_pin(in->target, NULL));
  }

  hpx_parcel_t *p = hpx_parcel_acquire(NULL, size);
  dbg_assert_str(p != NULL,"failed to allocate a parcel in eager receive\n");
  void *to = pwc_network_offset(p);

  const uint32_t bytes = pwc_network_size(p);
  dbg_assert_str(i + bytes < rx->size, "buffer should have wrapped\n");
  memcpy(to, from, bytes);

  // Mark the source of the parcel, based on the peer's rank.
  p->src = rx->peer->rank;

  log_net("sequence %lu: receiving %u-bytes from %d at offset %p\n",
          p->sequence, bytes, rx->peer->rank, from);

  // Before we leave, check some basics.
  dbg_assert(hpx_gas_try_pin(p->target, NULL));

  static const int class = HPX_INST_CLASS_NETWORK_PWC;
  static const int id = HPX_INST_EVENT_NETWORK_PWC_RECV;
  inst_trace(class, id, p->sequence, bytes, (uint64_t)from, rx->peer->rank);

  // Update the progress in this buffer.
  rx->min += bytes;
  log_net("updated min to %lu\n", rx->min);

  return p;
}
