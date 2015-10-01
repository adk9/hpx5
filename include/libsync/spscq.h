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

#ifndef SYNC_SPSCQ_H
#define SYNC_SPSCQ_H

#include <stdint.h>
#include <hpx/attributes.h>

/// This is a Single-Producer-Single-Consumer Concurrent Queue, implemented as a
/// concurrent bounded buffer.
///
/// This is a basic bounded buffer. We keep an N element in-place array that is
/// indexed (mod N) by the volatile head and tail pointers. The enqueue
/// operation checks to make sure that there is space in the array for the
/// insert, performs the insert, and increments the head. The dequeue operation
/// checks to see if the queue is empty, otherwise returns the element at tail
/// and increments tail.
///
/// In this implementation, we're careful to pad data in a way that we share as
/// little data between the head and tail as possible. We only enqueue and
/// dequeue data one element at a time, but we keep a cached value of the last
/// head and tail values that we have seen, so that we occasionally don't have
/// to read the remote data during an operation.
///
typedef struct sync_spscq_buffer sync_spscq_buffer_t;

typedef struct {
  struct {
    volatile uint32_t index;
    uint32_t cached;
    uint32_t capacity;
    const char pad[HPX_CACHELINE_SIZE - 12];
  } head;
  struct {
    volatile uint32_t index;
    uint32_t cached;
    const char pad[HPX_CACHELINE_SIZE - 8];
  } tail;
  sync_spscq_buffer_t * volatile buffer;
  const char pad[HPX_CACHELINE_SIZE - sizeof(sync_spscq_buffer_t *)];
} sync_spscq_t HPX_ALIGNED(HPX_CACHELINE_SIZE);

#define SYNC_SPSCQ_INIT { \
    .head = {             \
      .index = 0,         \
      .cached = 0,        \
      .capacity = 0,      \
      .pad = {0}          \
    },                    \
    .tail = {             \
      .index = 0,         \
      .cached = 0,        \
      .pad = {0}          \
    },                    \
  .buffer = NULL,         \
     .pad = {0}           \
  }

#define SYNC_SPSCQ_OK 0
#define SYNC_SPSCQ_EMPTY 1
#define SYNC_SPSCQ_FULL 1

/// Initialize a single-producer, single-consumer queue with at least enough
/// space for @p n messages.
///
/// @param            q The queue.
/// @param            n The initial queue size.
///
/// @returns SYNC_SPSCQ_OK for success otherwise returns ENOMEM.
int sync_spscq_init(sync_spscq_t *q, uint32_t n)
  HPX_NON_NULL(1) HPX_PUBLIC;


/// Finalize a single-producer, single-consumer queue.
///
/// @param           q The queue.
void sync_spscq_fini(sync_spscq_t *q)
  HPX_NON_NULL(1) HPX_PUBLIC;


/// Try to enqueue a message.
///
/// @param            q The queue.
/// @param         data The item to enqueue.
///
/// @returns SYNC_SPSCQ_OK or SYNC_SPSCQ_FULL.
int sync_spscq_try_enqueue(sync_spscq_t *q, void *data)
  HPX_NON_NULL(1) HPX_PUBLIC;


/// Enqueue a message.
///
/// This could try to insert for a while and/or potentially grow the buffer if
/// necessary.
///
/// @param           q The queue.
/// @param        data The item to enqueue.
///
/// @returns SYNC_SPSCQ_OK or ENOMEM.
int sync_spscq_enqueue(sync_spscq_t *q, void *data)
  HPX_NON_NULL(1) HPX_PUBLIC;


/// Try to dequeue a message.
///
/// @param            q The queue.
/// @param         data The pointer to deque.
///
/// @returns SYNC_SPSCQ_OK or SYNC_SPSCQ_EMPTY.
int sync_spscq_try_dequeue(sync_spscq_t *q, void **data)
  HPX_NON_NULL(1, 2) HPX_PUBLIC;

static inline void *sync_spscq_dequeue(sync_spscq_t *q) {
  void *data = NULL;
  sync_spscq_try_dequeue(q, &data);
  return data;
}

#endif
