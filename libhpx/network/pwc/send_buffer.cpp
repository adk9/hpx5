// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2016, Trustees of Indiana University,
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

#include "send_buffer.h"
#include "parcel_emulation.h"
#include <libhpx/debug.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>

using namespace libhpx::network::pwc;

/// The record type for the pending send circular buffer.
namespace {
struct Record {
  const hpx_parcel_t *p;
};
}

/// Append a record to the parcel's pending send buffer.
///
/// @param        sends The send buffer.
/// @param            p The parcel to buffer.
///
/// @returns  LIBHXP_OK The parcel was buffered successfully.
///        LIBHPX_ERROR A pending record could not be allocated.
static int
_append(send_buffer_t *sends, const hpx_parcel_t *p)
{
  Record *r = static_cast<Record*>(sends->pending.append());
  dbg_assert_str(r, "could not append a send operation to the buffer\n");
  r->p = p;
  return LIBHPX_OK;
}

static int
_start(send_buffer_t *sends, const hpx_parcel_t *p)
{
  return parcel_emulator_send(sends->emul, sends->xport, sends->rank, p);
}

/// Wrap the eager_buffer_tx() operation in an interface that matches the
/// circular_buffer_progress callback type.
static int
_start_record(void *buffer, void *record)
{
  send_buffer_t *sends = static_cast<send_buffer_t*>(buffer);
  Record *r = static_cast<Record*>(record);
  return _start(sends, r->p);
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
int
libhpx::network::pwc::send_buffer_progress(send_buffer_t *sends)
{
  int status = HPX_SUCCESS;
  sync_tatas_acquire(&sends->lock);
  int i = sends->pending.progress(_start_record, sends);
  if (i < 0) {
    log_error("failed to progress the send buffer\n");
    status = HPX_ERROR;
  }
  sync_tatas_release(&sends->lock);
  return status;
}

int
libhpx::network::pwc::send_buffer_init(send_buffer_t *sends, unsigned rank,
                                       parcel_emulator_t *emul,
                                       pwc_xport_t *xport, uint32_t size)
{
  sync_tatas_init(&sends->lock);
  sends->rank = rank;
  sends->emul = emul;
  sends->xport = xport;
  sends->pending.init(sizeof(Record), size);
  return HPX_SUCCESS;
}

void
libhpx::network::pwc::send_buffer_fini(send_buffer_t *sends)
{
  sends->pending.fini();
}

int
libhpx::network::pwc::send_buffer_send(send_buffer_t *sends,
                                       const hpx_parcel_t *p)
{
  int status = LIBHPX_OK;
  sync_tatas_acquire(&sends->lock);

  // If we have no pending sends, try and start a request.
  if (sends->pending.size() == 0) {
    status = _start(sends, p);
    if (status == LIBHPX_OK) {
      goto unlock;
    }

    // If we have an error at this point then report it and buffer the parcel.
    if (status != LIBHPX_RETRY) {
      log_error("error in parcel send, buffer the operation\n");
    }
  }

  // We need to buffer this parcel, because either we're already buffering
  // parcels, or we need to buffer while the parcel transport refreshes.
  status = _append(sends, p);
  dbg_check(status, "could not append send operation\n");

 unlock:
  sync_tatas_release(&sends->lock);
  return status;
}
