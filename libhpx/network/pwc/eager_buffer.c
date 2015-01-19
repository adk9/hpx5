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

static hpx_action_t _eager_rx = 0;
static hpx_action_t _finish_eager_tx = 0;

static int _handle_eager_rx(int *src) {
  uint32_t bytes = pgas_gpa_to_offset(hpx_thread_current_target());
  peer_t *peer = pwc_get_peer(here->network, *src);
  eager_buffer_t *eager = &peer->rx;
  hpx_parcel_t *parcel = eager_buffer_rx(eager, bytes);
  dbg_log_net("received %u eager parcel bytes from %d\n", bytes, *src);
  scheduler_spawn(parcel);
  return LIBHPX_OK;
}

static int _handle_finish_eager_tx(void *UNUSED) {
  hpx_parcel_t *p = NULL;
  hpx_addr_t target = hpx_thread_current_target();
  if (!hpx_gas_try_pin(target, (void**)&p)) {
    return dbg_error("could not finish eager tx\n");
  }
  dbg_log_net("releasing sent parcel\n");
  hpx_parcel_release(p);
  hpx_gas_unpin(target);
  return LIBHPX_OK;
}

static HPX_CONSTRUCTOR void _init_handlers(void) {
  LIBHPX_REGISTER_INTERRUPT(_handle_eager_rx, &_eager_rx);
  LIBHPX_REGISTER_INTERRUPT(_handle_finish_eager_tx, &_finish_eager_tx);
}

static uint32_t _index_of(eager_buffer_t *buffer, uint64_t i) {
  return (i & (buffer->size - 1));
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

int eager_buffer_tx(eager_buffer_t *buffer, hpx_parcel_t *p) {
  const uint32_t n = parcel_network_size(p);
  if (n > buffer->size) {
    return dbg_error("cannot send %u bytes via eager parcel buffer\n", n);
  }

  const uint64_t end = buffer->max + n;
  if (end > (1ull << GPA_OFFSET_BITS)) {
    dbg_error("lifetime send buffer overflow handling unimplemented\n");
    return LIBHPX_EUNIMPLEMENTED;
  }

  if (end - buffer->min > buffer->size) {
    dbg_log("could not send %u byte parcel\n", n);
    return LIBHPX_RETRY;
  }

  const uint32_t roff = _index_of(buffer, buffer->max);
  const uint32_t eoff = _index_of(buffer, end);
  const int wrapped = (eoff <= roff);
  if (wrapped) {
    dbg_error("wrapped parcel send buffer handling unimplemented\n");
    return LIBHPX_EUNIMPLEMENTED;
  }

  buffer->max += n;

  const void *lva = parcel_network_offset(p);
  const hpx_addr_t parcel = lva_to_gva(p);
  assert(parcel != HPX_NULL);
  const completion_t lsync = encode_completion(_finish_eager_tx, parcel);
  const completion_t completion = encode_completion(_eager_rx, n);
  dbg_log_net("sending %d byte parcel\n", n);
  return peer_pwc(buffer->peer, roff + buffer->base, lva, n, lsync, HPX_NULL,
                  completion, SEGMENT_EAGER);
}

hpx_parcel_t *eager_buffer_rx(eager_buffer_t *buffer, uint32_t bytes) {
  // allocate a parcel to copy out to
  const uint32_t size = bytes - parcel_prefix_size();
  hpx_parcel_t *p = hpx_parcel_acquire(NULL, size);
  DEBUG_IF(!p) {
    dbg_error("failed to allocate a parcel in eager receive\n");
  }

  // copy the parcel data
  const uint32_t i = _index_of(buffer, buffer->min);
  const uint64_t offset = buffer->base + i;
  const void *from = buffer->peer->segments[SEGMENT_EAGER].base + offset;
  memcpy(parcel_network_offset(p), from, bytes);

  // update the progress in this buffer
  buffer->min += bytes;

  // fill in the parcel source data and return
  p->src = buffer->peer->rank;
  return p;
}
