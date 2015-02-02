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

#include <stdlib.h>
#include <hpx/builtins.h>
#include <libhpx/debug.h>
#include <libhpx/libhpx.h>
#include <libhpx/parcel.h>
#include "irecv_buffer.h"
#include "parcel_utils.h"

#define ACTIVE_RANGE_CHECK(irecvs, i, R) do {                       \
  irecv_buffer_t *_irecvs = (irecvs);                               \
  int _i = i;                                                       \
  DEBUG_IF (_i < 0 || _i > _irecvs->n) {                            \
    dbg_error("index %i out of range [0, %u)\n", _i, _irecvs->n);   \
    return R;                                                       \
  }                                                                 \
  } while (0)


/// Cancel an active irecv request.
static hpx_parcel_t *_cancel(irecv_buffer_t *buffer, int i) {
  ACTIVE_RANGE_CHECK(buffer, i, NULL);

  if (MPI_SUCCESS != MPI_Cancel(buffer->requests + i)) {
    dbg_error("could not cancel MPI request\n");
    return NULL;
  }

  MPI_Status status;
  if (MPI_SUCCESS != MPI_Wait(buffer->requests + i, &status)) {
    dbg_error("could not cleanup a canceled MPI request\n");
    return NULL;
  }

  int cancelled;
  if (MPI_SUCCESS != MPI_Test_cancelled(&status, &cancelled)) {
    dbg_error("could not test a status to see if a request was canceled\n");
    return NULL;
  }

  if (cancelled) {
    hpx_gas_free(buffer->records[i].handler, HPX_NULL);
    hpx_parcel_release(buffer->records[i].parcel);
    return NULL;
  }
  else {
    hpx_gas_free(buffer->records[i].handler, HPX_NULL);
    return buffer->records[i].parcel;
  }
}


/// Start an irecv for a buffer entry.
///
/// @param       irecvs The irecv buffer.
/// @param            i The index we're regenerating.
///
/// @returns  LIBHPX_OK The irecv started correctly.
///        LIBHPX_ERROR There was an error during the operation.
static int _start(irecv_buffer_t *irecvs, int i) {
  ACTIVE_RANGE_CHECK(irecvs, i, LIBHPX_ERROR);

  // allocate a parcel buffer for this tag type---this will be the maximum
  // payload for this class of parcels
  int tag = irecvs->records[i].tag;
  uint32_t payload = tag_to_payload_size(tag);
  hpx_parcel_t *p = hpx_parcel_acquire(NULL, payload);

  const int src = MPI_ANY_SOURCE;
  const MPI_Comm com = MPI_COMM_WORLD;
  MPI_Request *r = irecvs->requests + i;
  int n = payload_size_to_mpi_bytes(payload);
  void *b = parcel_network_offset(p);

  if (MPI_SUCCESS != MPI_Irecv(b, n, MPI_BYTE, src, tag, com, r)) {
    return dbg_error("could not start irecv\n");
  }
  else {
    irecvs->records[i].parcel = p;
    log_net("started an MPI_Irecv operation: %u bytes\n", n);
    return LIBHPX_OK;
  }
}


/// Resize an irecv buffer to the requested size.
///
/// Buffer sizes can either be increased or decreased. The increase in size is
/// saturated by the buffers limit, unless the limit is 0 in which case
/// unbounded growth is permitted. A decrease in size could cancel active
/// irecvs, which may in tern match real sends, so this routine returns
/// successfully received parcels.
///
/// @param       buffer The buffer to resize.
/// @param         size The new size for the buffer.
/// @param[out]     out A chain of parcels from completed requests.
///
/// @returns  LIBHPX_OK The resize completed successfully.
///        LIBHPX_ERROR An error occurred while resizing.
int _resize(irecv_buffer_t *buffer, uint32_t size, hpx_parcel_t **out) {
  for (int i = size, e = buffer->n; i < e; ++i) {
    hpx_parcel_t *p = _cancel(buffer, i);
    if (out && p) {
      parcel_stack_push(out, p);
    }
  }

  if (buffer->limit && buffer->limit < size) {
    log_net("reducing expansion from %u to %u\n", size , buffer->limit);
    size = buffer->limit;
  }

  if (size == buffer->size) {
    return LIBHPX_OK;
  }

  buffer->requests = realloc(buffer->requests, size * sizeof(MPI_Request));
  buffer->statuses = realloc(buffer->statuses, size * sizeof(MPI_Status));
  buffer->out = realloc(buffer->out, size * sizeof(int));
  buffer->records = realloc(buffer->records, size * sizeof(*buffer->records));

  if (!size) {
    assert(!buffer->requests);
    assert(!buffer->statuses);
    assert(!buffer->out);
    assert(!buffer->records);
    return LIBHPX_OK;
  }

  if (buffer->requests && buffer->statuses && buffer->out && buffer->records) {
    log_net("buffer resized from %u to %u\n", buffer->size, size);
    buffer->size = size;
    return LIBHPX_OK;
  }

  return dbg_error("failed to resize buffer from %u to %u", buffer->size, size);
}


/// Append an irecv request and record for a given tag.
///
/// This starts the appended request.
///
/// @param       irecvs The buffer to append to.
/// @param          tag The tag type we want to append.
///
/// @returns  LIBHPX_OK The request was appended successfully.
///        LIBHPX_ERROR We overran the limit.
static int _append(irecv_buffer_t *irecvs, int tag) {
  // acquire an entry
  uint32_t n = irecvs->n++;
  if (n >= irecvs->size) {
    uint32_t limit = irecvs->limit;
    uint32_t size = 2 * irecvs->size;
    if (limit && limit < size) {
      return dbg_error("failed to extend irecvs (limit %u)\n", limit);
    }
    if (LIBHPX_OK != _resize(irecvs, size, NULL)) {
      return dbg_error("failed to extend irecvs (limit %u)\n", limit);
    }
  }

  // initialize the request
  irecvs->requests[n] = MPI_REQUEST_NULL;
  irecvs->records[n].tag = tag;
  irecvs->records[n].parcel = NULL;
  irecvs->records[n].handler = HPX_NULL;

  // start the irecv
  return _start(irecvs, n);
}


/// Probe for unexpected messages.
///
/// We do not have prior knowledge of the message sizes that we will receive. In
/// order to detect messages that have not yet matched an active irecv, we use
/// the MPI_Iprobe operation.
///
/// @param       irecvs The irecv buffer.
///
/// @returns  LIBHPX_OK The call completed successfully.
///        LIBHPX_ERROR We encountered an MPI error during the Iprobe.
static int _probe(irecv_buffer_t *irecvs) {
  const int src = MPI_ANY_SOURCE;
  const int tag = MPI_ANY_TAG;
  const MPI_Comm com = MPI_COMM_WORLD;
  int flag;
  MPI_Status s;
  if (MPI_SUCCESS != MPI_Iprobe(src, tag, com, &flag, &s)) {
    return dbg_error("failed MPI_Iprobe\n");
  }

  if (!flag) {
    return LIBHPX_OK;
  }

  // There's a pending message that we haven't matched yet. Append a record to
  // match it in the future.
  log_net("probe detected irecv for %u-byte parcel\n",
              tag_to_payload_size(s.MPI_TAG));
  return _append(irecvs, s.MPI_TAG);
}


/// Finish an irecv operation.
///
/// This extracts and finishes the parcel and regenerates the irecv.
///
/// @param       buffer The buffer.
/// @param            i The index to finish.
///
/// @returns            The parcel that we received.
static hpx_parcel_t *_finish(irecv_buffer_t *irecvs, int i, MPI_Status *s) {
  ACTIVE_RANGE_CHECK(irecvs, i, NULL);

  if (s->MPI_ERROR != MPI_SUCCESS) {
    dbg_error("irecv failed\n");
    return NULL;
  }

  int n;
  if (MPI_SUCCESS != MPI_Get_count(s, MPI_BYTE, &n)) {
    dbg_error("could not extract the size of an irecv\n");
    return NULL;
  }

  assert(n > 0);
  assert(irecvs->records[i].handler == HPX_NULL);

  hpx_parcel_t *p = irecvs->records[i].parcel;
  p->src = s->MPI_SOURCE;
  p->size = mpi_bytes_to_payload_size(n);
  log_net("finished a recv for a %u-byte payload\n", p->size);
  if (LIBHPX_OK != _start(irecvs, i)) {
    dbg_error("failed to regenerate an irecv\n");
  }
  return p;
}


int irecv_buffer_init(irecv_buffer_t *buffer, uint32_t size, uint32_t limit) {
  buffer->limit = limit;
  buffer->size = 0;
  buffer->n = 0;

  buffer->requests = NULL;
  buffer->statuses = NULL;
  buffer->out = NULL;
  buffer->records = NULL;

  int e = _resize(buffer, size, NULL);
  if (LIBHPX_OK != e) {
    irecv_buffer_fini(buffer);
  }
  return e;
}


void irecv_buffer_fini(irecv_buffer_t *buffer) {
  hpx_parcel_t *chain = NULL;
  if (LIBHPX_OK != _resize(buffer, 0, &chain)) {
    dbg_error("failed to fini the irecv buffer");
  }

  hpx_parcel_t *p = NULL;
  while ((p = parcel_stack_pop(&chain))) {
    hpx_parcel_release(p);
  }

  assert(!buffer->records);
  assert(!buffer->out);
  assert(!buffer->statuses);
  assert(!buffer->requests);
}


hpx_parcel_t *irecv_buffer_progress(irecv_buffer_t *buffer) {
  // see if there is an existing message we're not ready to receive
  if (LIBHPX_OK != _probe(buffer)) {
    dbg_error("failed probe\n");
  }

  // test the existing irecvs
  int n = buffer->n;
  if (!n) {
    return NULL;
  }

  int *out = buffer->out;
  MPI_Request *reqs = buffer->requests;
  MPI_Status *stats = buffer->statuses;

  int count;
  if (MPI_SUCCESS != MPI_Testsome(n, reqs, &count, out, stats)) {
    dbg_error("failed MPI_Testsome\n");
    return NULL;
  }

  hpx_parcel_t *completed = NULL;
  for (int i = 0; i < count; ++i) {
    int j = out[i];
    MPI_Status *s = stats + i;
    hpx_parcel_t *p = _finish(buffer, j, s);
    parcel_stack_push(&completed, p);
  }

  return completed;
}
