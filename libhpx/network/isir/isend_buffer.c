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


#include <hpx/builtins.h>
#include <libhpx/debug.h>
#include <libhpx/gas.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/parcel.h>
#include "isend_buffer.h"
#include "parcel_utils.h"


/// Compute the buffer index of an abstract index.
///
/// @param            i The abstract index.
/// @param            n The size of the buffer (must be 2^k).
///
/// @returns            The index in the buffer for this abstract index.
static int _index_of(uint64_t i, uint32_t n) {
  return (i & (n - 1));
}


/// Re-size an isend buffer to the requested size.
///
/// Buffer sizes can only be increased in the current implementation. The size
/// must be a power of 2.
///
/// @param       buffer The buffer to re-size.
/// @param         size The new size.
///
/// @returns  LIBHPX_OK The buffer was resized correctly.
///       LIBHPX_ENOMEM There was not enough memory to resize the buffer.
static int _resize(isend_buffer_t *buffer, uint32_t size) {
  if (size < buffer->size) {
    return dbg_error("cannot shrink send buffer\n");
  }

  if (size == buffer->size) {
    return LIBHPX_OK;
  }

  // start by resizing the buffers
  uint32_t oldsize = buffer->size;
  buffer->size = size;
  buffer->requests = realloc(buffer->requests, size * sizeof(MPI_Request));
  buffer->out = realloc(buffer->out, size * sizeof(int));
  buffer->records = realloc(buffer->records, size * sizeof(*buffer->records));

  if (!buffer->requests || !buffer->out || !buffer->records) {
    return dbg_error("failed to resize a send buffer from %u to %u\n",
                     oldsize, size);
  }

  int n = buffer->max - buffer->min;
  if (!n) {
    goto exit;
  }

  // Resizing the buffer changes where our index mapping is, we need to move
  // data around in the arrays. We do that by memcpy-ing either the prefix or
  // suffix of a wrapped buffer into the new position. After resizing the buffer
  // should never be wrapped.
  int min = _index_of(buffer->min, oldsize);
  int max = _index_of(buffer->max, oldsize);
  int prefix = (min < max) ? max - min : oldsize - min;
  int suffix = (min < max) ? 0 : max;

  int nmin = _index_of(buffer->min, size);
  int nmax = _index_of(buffer->max, size);

  // This code is slightly tricky. We only need to move one of the ranges,
  // either [min, oldsize) or [0, max). We determine which range we need to move
  // by seeing if the min or max index is different in the new buffer, and then
  // copying the appropriate bytes of the requests and records arrays to the
  // right place in the new buffer.
  if (min == nmin) {
    assert(max != nmax);
    assert(0 < suffix);
    assert(min + prefix == nmax - suffix);

    size_t bytes = suffix * sizeof(*buffer->requests);
    memcpy(buffer->requests + min + prefix, buffer->requests, bytes);
    bytes = suffix * sizeof(*buffer->records);
    memcpy(buffer->records + min + prefix, buffer->records, bytes);
  }
  else if (max == nmax) {
    assert(0 < prefix);
    assert(nmin + prefix <= size);
    size_t bytes = prefix * sizeof(*buffer->requests);
    memcpy(buffer->requests + nmin, buffer->requests + min, bytes);
    bytes = prefix * sizeof(*buffer->records);
    memcpy(buffer->records + nmin, buffer->records + min, bytes);
  }
  else {
    return dbg_error("unexpected shift in isend buffer _resize\n");
  }

 exit:
  dbg_log_net("resized a send buffer from %u to %u\n", oldsize, size);
  return LIBHPX_OK;
}


/// Start an isend operation.
///
/// @precondition There must be a valid entry in the buffer that is not yet
///               active.
///
/// @param       isends The buffer to start the send from.
/// @param            i The index to start.
///
/// @returns  LIBHPX_OK success
///        LIBHPX_ERROR MPI error
static int _start(isend_buffer_t *isends, int i) {
  assert(0 <= i && i < isends->size);

  hpx_parcel_t *p = isends->records[i].parcel;

  MPI_Request *r = isends->requests + i;
  void *from = mpi_offset_of_parcel(p);
  int to = gas_owner_of(here->gas, p->target);
  int n = payload_size_to_mpi_bytes(p->size);
  int tag = payload_size_to_tag(p->size);


  if (MPI_SUCCESS != MPI_Isend(from, n, MPI_BYTE, to, tag, MPI_COMM_WORLD, r)) {
    return dbg_error("failed MPI_Isend: %u bytes to %d\n", n, to);
  }

  dbg_log_net("started MPI_Isend: %u bytes to %d\n", n, to);
  return LIBHPX_OK;
}


/// Start as many isend operations as we can.
///
/// @param       isends The buffer from which to start isends.
///
/// @returns            The number of sends that remain unstarted.
int _start_all(isend_buffer_t *isends) {
  uint32_t size = isends->size;
  uint64_t max = isends->max;
  uint64_t limit = isends->limit;
  uint64_t end = (limit) ? min_uint_64(isends->min + limit, max) : max;

  for (uint64_t i = isends->active; i < end; ++i) {
    int j = _index_of(i, size);
    int e = _start(isends, j);
    dbg_check(e, "Failed to start an Isend operation.\n");
  }
  isends->active = end;
  return (max - end);
}


/// Test a contiguous range of the buffer.
///
/// This performs a single MPI_Testsome() on a range of requests covering
/// [i, i + n), and calls _finish() for each request that is completed.
///
/// @param       buffer The buffer to test.
/// @param            i The physical index at which the range starts.
/// @param            n The number of sends to test.
///
/// @returns            The number of completed requests in this range.
static int _test_range(isend_buffer_t *buffer, uint32_t i, uint32_t n, int o) {
  assert(0 <= i && i + n <= buffer->size);

  if (n == 0)
    return 0;

  int cnt = 0;
  MPI_Request *requests = buffer->requests + i;
  int *out = buffer->out + o;
  if (MPI_SUCCESS != MPI_Testsome(n, requests, &cnt, out, MPI_STATUS_IGNORE)) {
    dbg_error("MPI_Testsome error is fatal.\n");
    return 0;
  }

  DEBUG_IF(cnt == MPI_UNDEFINED) {
    dbg_error("Silent MPI_Testsome error detected\n");
    return 0;
  }

  for (int j = 0; j < cnt; ++j) {
    out[j] += i;
    int k = out[j];
    assert(i <= k && k < i + n);

    // handle each of the completed requests
    hpx_parcel_release(buffer->records[k].parcel);
    hpx_gas_free(buffer->records[k].handler, HPX_NULL);
  }

  return cnt;
}


/// Compact the buffer after testing.
///
/// @param       buffer The buffer to compact.
/// @param            n The number of valid entries in buffer->out.
static void _compact(isend_buffer_t *buffer, int n) {
  uint32_t size = buffer->size;
  uint64_t m = buffer->active - buffer->min;
  if (n == m) {
    dbg_log_net("bulk compaction of %d/%lu sends in buffer (%u)\n", n, m,
                size);
    buffer->min += n;
    return;
  }

  // incremental compaction
  for (int i = 0; i < n; ++i) {
    int j = buffer->out[i];
    DEBUG_IF(true) {
      buffer->requests[j] = NULL;
    }

    uint64_t min = buffer->min++;
    int k = _index_of(min, size);
    buffer->requests[j] = buffer->requests[k];
    buffer->records[j] = buffer->records[k];
  }

  dbg_log_net("incremental compaction of %d/%lu sends in buffer (%u)\n", n, m,
              size);
}


/// Test all of the active isend operations.
///
/// The isend buffer is a standard circular buffer, so we need to test one or
/// two ranges, depending on if the buffer is currently wrapped.
///
/// @param       buffer The buffer to test.
///
/// @returns            The number of sends completed.
static int _test_all(isend_buffer_t *buffer) {
  uint32_t size = buffer->size;
  uint32_t i = _index_of(buffer->min, size);
  uint32_t j = _index_of(buffer->active, size);

  // might have to test two ranges, [i, size) and [0, j), or just [i, j)
  bool wrapped = (j < i);
  uint32_t n = (wrapped) ? buffer->size - i : j - i;
  uint32_t m = (wrapped) ? j : 0;

  int total = 0;
  total += _test_range(buffer, i, n, total);
  total += _test_range(buffer, 0, m, total);
  if (total) {
    _compact(buffer, total);
  }
  dbg_log_net("tested %u sends, completed %d\n", n+m, total);
  return total;
}


/// Cancel an active request.
///
/// This is synchronous, and will wait until the request has been canceled.
///
/// @param       buffer The buffer.
/// @param            i The index to cancel.
///
/// @returns  LIBHPX_OK The request was successfully canceled.
///        LIBHPX_ERROR Three was an error during the operation.
static int _cancel(isend_buffer_t *buffer, int i) {
  assert(0 <= i && i < buffer->size);
  assert(false);

  if (MPI_SUCCESS != MPI_Cancel(buffer->requests + i)) {
    return dbg_error("could not cancel MPI request\n");
  }

  MPI_Status status;
  if (MPI_SUCCESS != MPI_Wait(buffer->requests + i, &status)) {
    return dbg_error("could not cleanup a canceled MPI request\n");
  }

  int cancelled;
  if (MPI_SUCCESS != MPI_Test_cancelled(&status, &cancelled)) {
    return dbg_error("could not test a status\n");
  }

  if (buffer->records) {
    hpx_gas_free(buffer->records[i].handler, HPX_NULL);
    hpx_parcel_release(buffer->records[i].parcel);
  }
  return LIBHPX_OK;
}


/// Cancel and cleanup all outstanding requests in the buffer.
///
/// @param       buffer The buffer.
static void _cancel_all(isend_buffer_t *buffer) {
  if (!buffer->requests) {
    return;
  }

  uint32_t size = buffer->size;
  for (uint64_t i = buffer->min, e = buffer->active; i < e; ++i) {
    if (LIBHPX_OK != _cancel(buffer, _index_of(i, size))) {
      dbg_error("failed to cancel pending sends\n");
      return;
    }
  }
}


int isend_buffer_init(isend_buffer_t *buffer, uint32_t size, uint32_t limit) {
  buffer->limit = limit;
  buffer->size = 0;
  buffer->min = 0;
  buffer->active = 0;
  buffer->max = 0;

  buffer->requests = NULL;
  buffer->out = NULL;
  buffer->records = NULL;

  size = 1 << ceil_log2_32(size);
  int e = _resize(buffer, size);
  if (LIBHPX_OK != e) {
    isend_buffer_fini(buffer);
  }
  return e;
}


void isend_buffer_fini(isend_buffer_t *buffer) {
  if (!buffer) {
    return;
  }

  _cancel_all(buffer);

  if (buffer->records) {
    free(buffer->records);
  }

  if (buffer->out) {
    free(buffer->out);
  }

  if (buffer->requests) {
    free(buffer->requests);
  }
}


int isend_buffer_append(isend_buffer_t *buffer, hpx_parcel_t *p, hpx_addr_t h)
{
  uint64_t i = buffer->max++;
  uint32_t size = buffer->size;
  if (size <= buffer->max - buffer->min) {
    size = 2 * size;
    if (LIBHPX_OK != _resize(buffer, size)) {
      return dbg_error("failed to resize isend buffer during append\n");
    }
  }

  int j = _index_of(i, size);
  assert(0 <= j && j < buffer->size);
  DEBUG_IF(true) {
    buffer->requests[j] = MPI_REQUEST_NULL;
  }
  buffer->records[j].parcel = p;
  buffer->records[j].handler = h;
  return LIBHPX_OK;
}


int isend_buffer_flush(isend_buffer_t *buffer) {
  int total = 0;
  do {
    total += isend_buffer_progress(buffer);
  } while (buffer->min != buffer->max);
  return total;
}


int isend_buffer_progress(isend_buffer_t *isends) {
  int m = _test_all(isends);
  DEBUG_IF (m) {
    dbg_log_net("finished %d sends\n", m);
  }

  int n = _start_all(isends);
  DEBUG_IF (n) {
    dbg_log_net("failed to start %d sends\n", n);
  }

  return m;

  // avoid unused errors.
  (void)n;
}
