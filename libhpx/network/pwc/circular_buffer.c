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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "hpx/builtins.h"
#include "libhpx/debug.h"
#include "libhpx/libhpx.h"
#include "circular_buffer.h"

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

/// Compute the address of an element in the buffer.
///
/// @param       buffer The buffer itself.
/// @param            i The element index.
///
/// @returns            The address of the element in the buffer.
static void *_address_of(circular_buffer_t *buffer, uint32_t i) {
  size_t n = i * buffer->esize;
  return (char*)buffer->records + n;
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
static int _reflow(circular_buffer_t *buffer, uint32_t oldsize) {
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
    size_t bytes = suffix * buffer->esize;
    void *to = _address_of(buffer, min + prefix);
    const void *from = _address_of(buffer, 0);
    memcpy(to, from, bytes);
  }
  else if (max == nmax) {
    assert(0 < prefix);
    assert(nmin + prefix <= size);
    size_t bytes = prefix * buffer->esize;
    void *to = _address_of(buffer, nmin);
    const void *from = _address_of(buffer, min);
    memcpy(to, from, bytes);
  }
  else {
    return dbg_error("unexpected shift\n");
  }

 exit:
  dbg_log_net("reflowed a circular buffer from %u to %u\n", oldsize, size);
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
static int _expand(circular_buffer_t *buffer, uint32_t size) {
  assert(size != 0);

  if (size < buffer->size) {
    return dbg_error("cannot shrink a circular buffer yet\n");
  }

  if (size == buffer->size) {
    return LIBHPX_OK;
  }

  uint32_t oldsize = buffer->size;
  buffer->size = size;
  buffer->records = realloc(buffer->records, size * buffer->esize);
  if (!buffer->records) {
    dbg_error("failed to resize a circular buffer (%u to %u)\n", oldsize, size);
    return LIBHPX_ENOMEM;
  }

  return _reflow(buffer, oldsize);
}

int circular_buffer_init(circular_buffer_t *b, uint32_t esize, uint32_t size) {
  b->size = 0;
  b->esize = esize;
  b->min = 0;
  b->max = 0;
  b->records = NULL;

  size = 1 << ceil_log2_32(size);
  return _expand(b, size);
}

void circular_buffer_fini(circular_buffer_t *b) {
  if (b->records) {
    free(b->records);
  }
}

void *circular_buffer_append(circular_buffer_t *buffer) {
  uint64_t next = buffer->max++;
  if (buffer->max - buffer->min >= buffer->size) {
    uint32_t size = 1 << buffer->size;
    if (LIBHPX_OK != _expand(buffer, size)) {
      dbg_error("could not expand a circular buffer from %u to %u\n",
                buffer->size, size);
      return NULL;
    }
  }
  uint32_t i = _index_of(next, buffer->size);
  return _address_of(buffer, i);
}

int circular_buffer_progress(circular_buffer_t *buffer,
                             int (*progress_callback)(void *env, void *record),
                             void *progress_env) {
  while (buffer->min < buffer->max) {
    uint32_t i = _index_of(buffer->min, buffer->size);
    void *record = _address_of(buffer, i);
    int e = progress_callback(progress_env, record);
    switch (e) {
     default:
      dbg_error("circular buffer could not progress\n");
      return -1;

     case LIBHPX_RETRY:
      return buffer->max - buffer->min;

     case LIBHPX_OK:
      ++buffer->min;
      break;
    }
  }
  return 0;
}
