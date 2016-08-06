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

#include "SendBuffer.h"
#include "libhpx/debug.h"
#include "libhpx/libhpx.h"

/// The record type for the pending send circular buffer.
namespace {
using libhpx::network::pwc::SendBuffer;
struct Record {
  const hpx_parcel_t *p;
};
}

SendBuffer::SendBuffer()
    : rank_(),
      UNUSED_PADDING_(),
      emul_(nullptr),
      xport_(),
      pending_()
{
}

int
SendBuffer::append(const hpx_parcel_t *p)
{
  Record *r = static_cast<Record*>(pending_.append());
  dbg_assert_str(r, "could not append a send operation to the buffer\n");
  r->p = p;
  return LIBHPX_OK;
}

int
SendBuffer::start(const hpx_parcel_t *p)
{
  return emul_->send(rank_, p);
}

/// Wrap the eager_buffer_tx() operation in an interface that matches the
/// circular_buffer_progress callback type.
int
SendBuffer::StartRecord(void *buffer, void *record)
{
  SendBuffer *sends = static_cast<SendBuffer*>(buffer);
  Record *r = static_cast<Record*>(record);
  return sends->start(r->p);
}

int
SendBuffer::progress()
{
  std::lock_guard<std::mutex> _(lock_);
  int i = pending_.progress(StartRecord, this);
  if (i < 0) {
    log_error("failed to progress the send buffer\n");
    return HPX_ERROR;
  }
  return HPX_SUCCESS;
}

void
SendBuffer::init(unsigned rank, ReloadParcelEmulator& emul, pwc_xport_t *xport)
{
  rank_ = rank;
  emul_ = &emul;
  xport_ = xport;
  pending_.init(sizeof(Record), 8);
}

void
SendBuffer::fini()
{
  pending_.fini();
}

int
SendBuffer::send(const hpx_parcel_t *p)
{
  std::lock_guard<std::mutex> _(lock_);
  int status = LIBHPX_OK;

  // If we have no pending sends, try and start a request.
  if (pending_.size() == 0) {
    status = start(p);
    if (status == LIBHPX_OK) {
      return status;
    }

    // If we have an error at this point then report it and buffer the parcel.
    if (status != LIBHPX_RETRY) {
      log_error("error in parcel send, buffer the operation\n");
    }
  }

  // We need to buffer this parcel, because either we're already buffering
  // parcels, or we need to buffer while the parcel transport refreshes.
  status = append(p);
  dbg_check(status, "could not append send operation\n");
  return status;
}
