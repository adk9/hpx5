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
#include <hpx/hpx.h>
#include <hpx/builtins.h>
#include <libhpx/debug.h>
#include <libhpx/libhpx.h>
#include <libhpx/parcel.h>
#include "buffers.h"


/// Compact the buffer by swapping the specified element with the min element.
static void _compact(buffer_t *buffer, uint32_t i) {
  // swap the min element
  uint32_t min = buffer->min++;
  uint32_t j = buffer_index_of(buffer, min);
  if (j != i) {
    buffer->requests[i] = buffer->requests[j];
    buffer->records[i] = buffer->records[j];
  }
}

static hpx_parcel_t *_cancel(buffer_t *buffer, uint32_t i) {
  record_t *r = &buffer->records[i];
  if (r->local) {
    hpx_lco_error(r->local, HPX_LCO_CANCEL, HPX_NULL);
  }
  return r->parcel;
}

static hpx_parcel_t *_finish(buffer_t *buffer, uint32_t i, MPI_Status *status) {
  DEBUG_IF(true) {
    buffer->requests[i] = MPI_REQUEST_NULL;
  }

  record_t *r = &buffer->records[i];
  if (r->local != HPX_NULL) {
    hpx_lco_set(r->local, 0, NULL, HPX_NULL, HPX_NULL);
  }

  return buffer->fini(r->parcel, status);
}


/// Wait for a single request to complete.
static hpx_parcel_t *_wait(buffer_t *buffer, uint32_t i) {
  int e;
  MPI_Status s;
  int cancelled;

  e = MPI_Wait(&buffer->requests[i], &s);
  dbg_check(e, "failed MPI_Test\n");

  e = MPI_Test_cancelled(&s, &cancelled);
  dbg_check(e, "Failed MPI_Test_cancelled\n");

  return (cancelled) ? _cancel(buffer, i) : _finish(buffer, i, &s);
}


/// Test a contiguous range of the buffer.
///
/// This performs a single MPI_Testsome() on a range of requests covering
/// [i, i + n), and calls _finish() for each request that is completed.
///
/// @param       buffer The buffer to test.
/// @param            i The physical index at which the range starts.
/// @param            n The number of sends to test.
/// @param[out]   stack A stack of parcels corresponding to completed requests.
///
/// @returns            The number of completed requests in this range.
static int _test_range(buffer_t *buffer, uint32_t i, uint32_t n,
                       hpx_parcel_t **parcels)
{
  if (!n)
    return 0;

  int cnt = 0;
  int out[n];
  MPI_Status status[n];
  int e = MPI_Testsome(n, &buffer->requests[i], &cnt, out, status);
  dbg_check(e, "failed MPI_Testsome\n");
  for (int j = 0; j < cnt; ++j) {
    int k = i + out[j];
    MPI_Status *s = &status[j];
    hpx_parcel_t *p = _finish(buffer, k, s);
    if (p) {
      parcel_stack_push(parcels, p);
    }
    _compact(buffer, k);
  }
  return cnt;
}


/// Tests all of the active requests in a buffer.
///
/// This deals with the wrapped condition for the @p isends buffer, where the
/// physical index of the min element is greater than the physical index of the
/// max element.
///
/// @param       buffer The buffer to test.
/// @param            f A function object to parcels.
/// @param[out] parcels A stack of parcels corresponding to completed requests.
///
/// @returns            The number of requests completed by this call.
int buffer_test_all(buffer_t *buffer, hpx_parcel_t **parcels) {
  uint32_t i = buffer_index_of(buffer, buffer->min);
  uint32_t j = buffer_index_of(buffer, buffer->active);

  // might have to test two ranges, [0, j) + [i, end), or just [i, j)
  bool wrapped = (j < i);
  uint32_t n = (wrapped) ? buffer->n - i : j - i;
  uint32_t m = (wrapped) ? j : 0;

  int total = 0;
  total += _test_range(buffer, i, n, parcels);
  total += _test_range(buffer, j, m, parcels);
  dbg_log_net("tested %u messages, finished %d\n", n+m, total);
  return total;
}


void buffer_double(buffer_t *buffer) {
  uint32_t n = buffer->n << 1;
  if (n < buffer->n) {
    dbg_error("overflowed max buffer capacity\n");
  }
  buffer->n = n;
  buffer->requests = realloc(buffer->requests, n * sizeof(buffer->requests[0]));
  buffer->records = realloc(buffer->records, n * sizeof(buffer->records[0]));
}


void buffer_append(buffer_t *isends, record_t record) {
  uint64_t n = isends->max++;
  if (isends->max - isends->min >= (uint64_t)isends->n) {
    buffer_double(isends);
  }
  uint32_t idx = buffer_index_of(isends, n);
  isends->requests[idx] = MPI_REQUEST_NULL;
  isends->records[idx] = record;
}


int buffer_init(buffer_t *buffer, uint32_t size, uint32_t limit, finalizer_t f) {
  buffer->n = 1 << ceil_log2_32(size);
  buffer->limit = limit;
  buffer->fini = f;
  buffer->min = 0;
  buffer->active = 0;
  buffer->max = 0;
  buffer->requests = calloc(buffer->n, sizeof(buffer->requests[0]));
  buffer->records = calloc(buffer->n, sizeof(buffer->records[0]));
  return LIBHPX_OK;
}


void buffer_fini(buffer_t *buffer) {
  // cancel all outstanding MPI requests
  for (uint64_t i = buffer->min, e = buffer->active; i < e; ++i) {
    int32_t j = buffer_index_of(buffer, i);
    MPI_Cancel(&buffer->requests[j]);
  }

  // wait for all outstanding requests (either canceled or completed)
  for (uint64_t i = buffer->min, e = buffer->active; i < e; ++i) {
    int32_t j = buffer_index_of(buffer, i);
    hpx_parcel_t *p = _wait(buffer, j);
    hpx_parcel_release(p);
  }

  // cancel any buffered requests
  for (uint64_t i = buffer->active, e = buffer->max; i < e; ++i) {
    uint32_t j = buffer_index_of(buffer, i);
    hpx_parcel_t *p = _cancel(buffer, j);
    hpx_parcel_release(p);
  }
}

