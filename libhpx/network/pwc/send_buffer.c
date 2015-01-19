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
  sync_tatas_init(&sends->lock);
  sends->tx = tx;
  return circular_buffer_init(&sends->pending, sizeof(record_t), size);
}

void send_buffer_fini(send_buffer_t *sends) {
  circular_buffer_fini(&sends->pending);
}

int send_buffer_send(send_buffer_t *sends, hpx_parcel_t *p, hpx_addr_t lsync) {
  if (lsync != HPX_NULL) {
    dbg_error("local send complete event unimplemented\n");
    return LIBHPX_EUNIMPLEMENTED;
  }

  int status = LIBHPX_OK;
  sync_tatas_acquire(&sends->lock);

  // Try an eager send if we don't have anything pending.
  if (circular_buffer_size(&sends->pending) == 0) {
    status = eager_buffer_tx(sends->tx, p);
    if (LIBHPX_OK == status) {
      goto unlock;
    }

    if (LIBHPX_RETRY != status) {
      status = dbg_error("error sending parcel\n");
      goto unlock;
    }
  }

  // We get here if there were already buffered sends, or if the network told us
  // to retry.
  record_t *r = circular_buffer_append(&sends->pending);
  if (!r) {
    status = dbg_error("could not append a send operation to the buffer\n");
    goto unlock;
  }

  // clear the retry signal
  status = LIBHPX_OK;

  r->p = p;
  r->lsync = lsync;

 unlock:
  sync_tatas_release(&sends->lock);
  return status;
}

/// Wrap the eager_buffer_tx() operation in an interface that matches the
/// circular_buffer_progress callback type.
static int _start_record(void *buffer, void *record) {
  send_buffer_t *sends = buffer;
  record_t *r = record;
  return eager_buffer_tx(sends->tx, r->p);
}

int send_buffer_progress(send_buffer_t *sends) {
  int i = 0;
  sync_tatas_acquire(&sends->lock);
  i = circular_buffer_progress(&sends->pending, _start_record, sends);
  if (i < 0) {
    dbg_error("failed to progress the send buffer\n");
  }
  sync_tatas_release(&sends->lock);
  return i;
}
