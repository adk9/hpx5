// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
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
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <hpx/builtins.h>
#include <libsync/sync.h>
#include <libsync/spscq.h>

struct sync_spscq_buffer {
  uint32_t           capacity;
  uint32_t               mask;
  sync_spscq_buffer_t *parent;
  void               *words[];
};

static sync_spscq_buffer_t *_buffer_new(sync_spscq_buffer_t *parent, uint32_t n) {
  assert(n);
  n = ceil_log2_32(n);
  sync_spscq_buffer_t *buffer = malloc(sizeof(*buffer) + n * sizeof(void*));
  if (buffer) {
    buffer->capacity = n;
    buffer->mask = n - 1;
    buffer->parent = parent;
  }
  return buffer;
}

static void _buffer_delete(sync_spscq_buffer_t *buffer) {
  if (!buffer)
    return;

  sync_spscq_buffer_t *parent = buffer->parent;
  free(buffer);
  _buffer_delete(parent);
}

static void _buffer_insert(sync_spscq_buffer_t *buffer, uint32_t i, void* val) {
  buffer->words[i % buffer->mask] = val;
}

static void *_buffer_lookup(const sync_spscq_buffer_t *buffer, uint32_t i) {
  return buffer->words[i % buffer->mask];
}

int sync_spscq_init(sync_spscq_t *q, uint32_t n) {
  sync_spscq_buffer_t *buffer = _buffer_new(NULL, n);
  if (!buffer)
    return ENOMEM;

  q->head.capacity = buffer->capacity;

  q->head.cached = 0;
  q->tail.cached = 0;


  sync_store(&q->buffer, buffer, SYNC_RELAXED);
  sync_store(&q->head.index, 0, SYNC_RELEASE);
  sync_store(&q->tail.index, 0, SYNC_RELEASE);

  return SYNC_SPSCQ_OK;
}

void sync_spscq_fini(sync_spscq_t *q) {
  assert(q);
  _buffer_delete(q->buffer);
}

int sync_spscq_try_enqueue(sync_spscq_t *q, void *data) {
  uint32_t capacity = q->head.capacity;
  uint32_t     head = q->head.index;
  uint32_t     tail = q->head.cached;

  // check if the queue seems to be full, and then check to see if it *really*
  // seems to be full
  if (tail + capacity <= head) {
    tail = q->head.cached = sync_load(&q->tail.index, SYNC_ACQUIRE);
    if (tail + capacity <= head)
      return SYNC_SPSCQ_FULL;
  }

  assert(head < UINT32_MAX);

  sync_spscq_buffer_t *buffer = sync_load(&q->buffer, SYNC_ACQUIRE);
  _buffer_insert(buffer, head++, data);
  sync_store(&q->head.index, head, SYNC_RELEASE);
  return SYNC_SPSCQ_OK;
}

int sync_spscq_enqueue(sync_spscq_t *q, void *data) {
  int result = sync_spscq_try_enqueue(q, data);
  while (result != SYNC_SPSCQ_OK) {
    assert(ceil_log2_32(q->head.capacity) < 32);
    q->head.capacity <<= 1;
    sync_spscq_buffer_t *old = sync_load(&q->buffer, SYNC_ACQUIRE);
    sync_spscq_buffer_t *new = _buffer_new(old, q->head.capacity);
    for (int i = q->head.cached, e = q->head.index; i < e; ++i)
      _buffer_insert(new, i, _buffer_lookup(old, i));
    (void)sync_swap(&q->buffer, new, SYNC_RELEASE);
    result = sync_spscq_try_enqueue(q, data);
  }
  return result;
}

int sync_spscq_try_dequeue(sync_spscq_t *q, void **data) {
  uint32_t     tail = q->tail.index;
  uint32_t     head = q->tail.cached;

  if (tail == head) {
    head = q->tail.cached = sync_load(&q->head.index, SYNC_ACQUIRE);
    if (tail == head)
      return SYNC_SPSCQ_EMPTY;
  }

  sync_spscq_buffer_t *buffer = sync_load(&q->buffer, SYNC_ACQUIRE);
  *data = _buffer_lookup(buffer, tail++);
  sync_store(&q->tail.index, tail, SYNC_RELEASE);
  return SYNC_SPSCQ_OK;
}
