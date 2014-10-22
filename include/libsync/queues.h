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
#ifndef LIBHPX_QUEUES_H
#define LIBHPX_QUEUES_H

/// ----------------------------------------------------------------------------
/// @file include/libsync/queues.h
/// ----------------------------------------------------------------------------
#include "hpx/attributes.h"
#include "cptr.h"

typedef struct queue {
  void (*delete)(struct queue *q) HPX_NON_NULL(1);
  void (*enqueue)(struct queue *q, void *val) HPX_NON_NULL(1);
  void *(*dequeue)(struct queue *q) HPX_NON_NULL(1);
} queue_t;

static inline void sync_queue_delete(queue_t *q) {
  q->delete(q);
}

static inline void sync_queue_enqueue(queue_t *q, void *val) {
  q->enqueue(q, val);
}

static inline void *sync_queue_dequeue(queue_t *q) {
  return q->dequeue(q);
}

/// M&S two lock queue from
/// http://www.cs.rochester.edu/u/scott/papers/1996_PODC_queues.pdf, implemented
/// based on
/// https://www.cs.rochester.edu/research/synchronization/pseudocode/queues.html#tlq

typedef struct two_lock_queue_node two_lock_queue_node_t;
struct two_lock_queue_node {
  two_lock_queue_node_t *next;
  void *value;
};


/// Using SWAP on the head and tail pointers for locking. Could use something
/// more scalable if higher contention.
typedef struct {
  queue_t vtable;
  const char *_paddinga[64 - sizeof(queue_t)];
  two_lock_queue_node_t *head;
  const char *_paddingb[64 - sizeof(two_lock_queue_node_t*)];
  two_lock_queue_node_t *tail;
  const char *_paddingc[64 - sizeof(two_lock_queue_node_t*)];
} HPX_ALIGNED(64) two_lock_queue_t;

two_lock_queue_t *sync_two_lock_queue_new(void) HPX_MALLOC;
void sync_two_lock_queue_delete(two_lock_queue_t *q) HPX_NON_NULL(1);

void sync_two_lock_queue_init(two_lock_queue_t *q, two_lock_queue_node_t *init)
  HPX_NON_NULL(1);

void sync_two_lock_queue_fini(two_lock_queue_t *q)
  HPX_NON_NULL(1);

void sync_two_lock_queue_enqueue(two_lock_queue_t *q, void *val)
  HPX_NON_NULL(1);

void *sync_two_lock_queue_dequeue(two_lock_queue_t *q)
  HPX_NON_NULL(1);

void sync_two_lock_queue_enqueue_node(two_lock_queue_t *q,
                                      two_lock_queue_node_t *node)
  HPX_NON_NULL(2);

two_lock_queue_node_t *sync_two_lock_queue_dequeue_node(two_lock_queue_t *q)
  HPX_NON_NULL(1);

typedef struct {
  queue_t vtable;
  volatile cptr_t head;
  volatile cptr_t tail;
} ms_queue_t;

ms_queue_t *sync_ms_queue_new(void) HPX_MALLOC;
void sync_ms_queue_delete(ms_queue_t *q) HPX_NON_NULL(1);

void  sync_ms_queue_init(ms_queue_t *q, void *val) HPX_NON_NULL(1);
void  sync_ms_queue_fini(ms_queue_t *q) HPX_NON_NULL(1);
void  sync_ms_queue_enqueue(ms_queue_t *q, void *val) HPX_NON_NULL(1);
void *sync_ms_queue_dequeue(ms_queue_t *q) HPX_NON_NULL(1);

#endif // LIBHPX_QUEUES_H
