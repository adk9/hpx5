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
#include <string.h>

#include <hpx/builtins.h>

#include <libhpx/debug.h>
#include <libhpx/libhpx.h>


#include "pwc_buffer.h"


/// Compute the index into the buffer for an abstract index.
///
/// The size must be 2^k because we mask instead of %.
///
/// @param            i The abstract index.
/// @param            n The size of the buffer.
///
/// @returns            i % size
static int _index_of(uint32_t i, uint32_t size) {
  return (i & (size - 1));
}


/// Reflow the elements in a buffer.
///
/// After resizing a buffer, existing elements are likely to be in the wrong
/// place, given the new size. This function will reflow the part of the buffer
/// that is in the wrong place.
///
/// @param       buffer The buffer to reflow.
/// @param      oldsize The size of the buffer prior to the resize operation.
///
/// @returns  LIBHPX_OK The reflow was successful.
///        LIBHPX_ERROR An unexpected error occurred.
static int _reflow(pwc_buffer_t *buffer, uint32_t oldsize) {
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

  uint32_t size = buffer->size;
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
    size_t bytes = suffix * sizeof(*buffer->records);
    memcpy(buffer->records + min + prefix, buffer->records, bytes);
  }
  else if (max == nmax) {
    assert(0 < prefix);
    assert(nmin + prefix <= size);
    size_t bytes = prefix * sizeof(*buffer->records);
    memcpy(buffer->records + nmin, buffer->records + min, bytes);
  }
  else {
    return dbg_error("unexpected shift in pwc buffer _reflow\n");
  }

 exit:
  dbg_log_net("reflowed a send buffer from %u to %u\n", oldsize, size);
  return LIBHPX_OK;
}


/// Expand the size of a buffer.
///
/// @param       buffer The buffer to expand.
/// @param         size The new size for the buffer.
///
/// @returns  LIBHPX_OK The buffer was expanded successfully.
///       LIBHPX_ENOMEM We ran out of memory during expansion.
///        LIBHPX_ERROR There was an unexpected error during expansion.
static int _expand(pwc_buffer_t *buffer, uint32_t size) {
  assert(size != 0);

  if (size < buffer->size) {
    return dbg_error("cannot string a pwc buffer\n");
  }

  if (size == buffer->size) {
    return LIBHPX_OK;
  }

  uint32_t oldsize = buffer->size;
  buffer->size = size;
  buffer->records = realloc(buffer->records, size * sizeof(pwc_record_t));
  if (buffer->records) {
    return _reflow(buffer, oldsize);
  }

  dbg_error("failed to resize a pwc buffer (%u to %u)\n", oldsize, size);
  return LIBHPX_ENOMEM;
}


/// Try to start a pwc.
static int _start(pwc_buffer_t *buffer, void *rva, const void *lva, size_t n,
                  hpx_addr_t local, hpx_addr_t remote, hpx_action_t op)
{
  if (remote != HPX_NULL) {
    dbg_error("remote receive event currently unsupported");
  }

  int flags = PHOTON_REQ_ONE_CQE |
              ((local) ? 0 : PHOTON_REQ_PWC_NO_LCE) |
              ((op) ? 0 : PHOTON_REQ_PWC_NO_RCE);

  int rank = buffer->rank;
  struct photon_buffer_priv_t key = buffer->key;
  void *vlva = (void*)lva;
  int e = photon_put_with_completion(rank, vlva, n, rva, key, local, op, flags);
  switch (e) {
   case PHOTON_OK:
    return LIBHPX_OK;
   case PHOTON_ERROR_RESOURCE:
    return LIBHPX_RETRY;
   default:
    return dbg_error("could not initiate a put-with-completion\n");
  }
}


/// Try to append a pwc request to be started in the future.
static int _append(pwc_buffer_t *buffer, void *rva, const void *lva, size_t n,
                   hpx_addr_t local, hpx_addr_t remote, hpx_action_t op)
{
  uint32_t size = buffer->size;
  if (size <= buffer->max - buffer->min) {
    size = size * 2;
    if (LIBHPX_OK != _expand(buffer, size)) {
      return dbg_error("failed to resize a pwc() buffer during append\n");
    }
  }

  uint64_t next = buffer->max++;
  uint32_t i = _index_of(next, size);
  buffer->records[i].rva = rva;
  buffer->records[i].lva = lva;
  buffer->records[i].n = n;
  buffer->records[i].local = local;
  buffer->records[i].remote = remote;
  buffer->records[i].op = op;
  return LIBHPX_OK;
}


int pwc_buffer_init(pwc_buffer_t *buffer, uint32_t rank, uint32_t size) {
  buffer->rank = rank;
  buffer->size = 0;
  buffer->min = 0;
  buffer->max = 0;
  buffer->records = NULL;

  size = 1 << ceil_log2_32(size);
  return _expand(buffer, size);
}


void pwc_buffer_fini(pwc_buffer_t *buffer) {
  if (!buffer) {
    return;
  }

  if (buffer->records) {
    free(buffer->records);
  }

  free(buffer);
}


int pwc_buffer_pwc(pwc_buffer_t *buffer, void *rva, const void *lva, size_t n,
                   hpx_addr_t local, hpx_addr_t remote, hpx_action_t op)
{
  if (pwc_buffer_progress(buffer) == 0) {
    int e = _start(buffer, rva, lva, n, local, remote, op);

    if (LIBHPX_OK == e) {
      return LIBHPX_OK;
    }

    if (LIBHPX_RETRY != e) {
      return dbg_error("pwc failed\n");
    }
  }

  if (LIBHPX_OK != _append(buffer, rva, lva, n, local, remote, op)) {
    return dbg_error("could not append pwc request\n");
  }

  return LIBHPX_OK;
}


int pwc_buffer_progress(pwc_buffer_t *buffer) {
  uint32_t size = buffer->size;
  while (buffer->min != buffer->max) {
    uint64_t next = buffer->min++;
    uint32_t i = _index_of(next, size);
    pwc_record_t *r = buffer->records + i;
    int e = _start(buffer, r->rva, r->lva, r->n, r->local, r->remote, r->op);

    if (LIBHPX_RETRY == e) {
      return buffer->max - buffer->min;
    }

    if (LIBHPX_OK != e) {
      return dbg_error("pwc failed\n");
    }
  }

  return 0;
}
