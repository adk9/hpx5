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

#include "IRecvBuffer.h"
#include "parcel_utils.h"
#include "libhpx/events.h"

namespace {
using libhpx::network::isir::IRecvBuffer;
}

IRecvBuffer::IRecvBuffer(Transport &xport, int limit)
    : xport_(xport),
      limit_(limit),
      capacity_(std::min(64, limit)),
      size_(0),
      requests_(capacity_),
      records_(capacity_)
{
}

IRecvBuffer::~IRecvBuffer()
{
  hpx_parcel_t *chain = reserve(0);
  while (hpx_parcel_t *p = parcel_stack_pop(&chain)) {
    parcel_delete(p);
  }
}

int
IRecvBuffer::progress(hpx_parcel_t** stack)
{
  assert(stack);
  probe();
  std::vector<int> out(size_);
  std::vector<Status> statuses(size_);
  int e = xport_.Testsome(size_, &requests_[0], &out[0], &statuses[0]);
  if (e) log_net("detected completed irecvs: %u\n", e);
  for (int i = 0; i < e; ++i) {
    auto p = finish(out[i], statuses[i]);
    parcel_stack_push(stack, p);
    start(out[i]);
  }
  return e;
}

hpx_parcel_t *
IRecvBuffer::reserve(unsigned capacity)
{
  hpx_parcel_t *out = nullptr;

  // saturate the capacity
  capacity_ = (limit_) ? std::min(capacity, limit_) : capacity;

  if (capacity == capacity_) {
    return out;
  }

  // cancel any excess irecvs
  for (auto i = size_ - 1; size_ > capacity_; --i, --size_) {
    if (auto* p = cancel(i)) {
      parcel_stack_push(&out, p);
    }
  }

  // resize the vectors
  requests_.reserve(capacity_);
  records_.reserve(capacity_);
  return out;
}

void
IRecvBuffer::start(unsigned i)
{
  auto& record = records_[i];
  assert(record.p == nullptr);
  auto tag = record.tag;
  auto size = tag_to_payload_size(tag);
  record.p = parcel_alloc(size);
  record.p->ustack = nullptr;
  record.p->next = nullptr;
  record.p->state = PARCEL_SERIALIZED;

  auto buffer = isir_network_offset(record.p);
  auto bytes = payload_size_to_isir_bytes(size);
  requests_[i] = xport_.irecv(buffer, bytes, tag);

  log_net("started an MPI_Irecv operation: tag %d , %u bytes\n", tag, bytes);
}

void
IRecvBuffer::probe()
{
  int tag = xport_.iprobe();
  if (tag < 0) {
    return;
  }

  log_net("detected a new send with tag: %u\n", tag);
  auto i = size_++;
  records_[i].tag = tag;
  start(i);
  if (i == capacity_) {
    reserve(2 * capacity_);
  }
}

hpx_parcel_t *
IRecvBuffer::cancel(unsigned i)
{
  assert(i < size_);
  auto& record = records_[i];
  auto* p = record.p;
  record.p = NULL;
  record.tag = -1;

  if (requests_[i].cancel()) {
    parcel_delete(p);
    p = NULL;
  }

  return p;
}

hpx_parcel_t*
IRecvBuffer::finish(unsigned i, Status& status)
{
  assert(i < size_);
  assert(status.bytes() > 0);
  Record& record = records_[i];
  auto p = record.p;
  record.p = nullptr;
  p->size = isir_bytes_to_payload_size(status.bytes());
  p->src = status.source();
  log_net("finished a recv for a %u-byte payload\n", p->size);
  EVENT_NETWORK_RECV();
  return p;
}
