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
#include "eager_buffer.h"
#include "send_buffer.h"

typedef struct {
  hpx_parcel_t  *p;
  hpx_addr_t lsync;
} record_t;


int send_buffer_init(send_buffer_t *sends, struct eager_buffer *tx,
                     uint32_t size)
{
  sends->tx = tx;
  return circular_buffer_init(&sends->pending, sizeof(record_t), size);
}

void send_buffer_fini(send_buffer_t *sends) {
  circular_buffer_fini(&sends->pending);
}

int send_buffer_send(send_buffer_t *sends, hpx_parcel_t *p, hpx_addr_t lsync) {
  // Before performing a send operation, we try and progress any pending
  // sends. The progress operation will return the number of remaining send
  // operations that are pending.
  if (send_buffer_progress(sends) == 0) {
    int e = eager_buffer_tx(sends->tx, p, lsync);
    if (LIBHPX_OK == e) {
      return e;
    }

    if (LIBHPX_RETRY != e) {
      return dbg_error("error sending parcel\n");
    }
  }

  // Either there are still pending sends, or the attempt to _start() returned a
  // retry signal, so buffer this send for now.
  record_t *r = circular_buffer_append(&sends->pending);
  if (!r) {
    return dbg_error("could not append a send operation to the buffer\n");
  }

  r->p = p;
  r->lsync = lsync;

  return LIBHPX_OK;
}

/// Wrap the eager_buffer_tx() operation in an interface that matches the
/// circular_buffer_progress callback type.
static int _start_record(void *buffer, void *record) {
  send_buffer_t *sends = buffer;
  record_t *r = record;
  return eager_buffer_tx(sends->tx, r->p, r->lsync);
}

int send_buffer_progress(send_buffer_t *sends) {
  int i = circular_buffer_progress(&sends->pending, _start_record, sends);
  if (i < 0) {
    dbg_error("failed to progress the send buffer\n");
  }
  return i;
}
