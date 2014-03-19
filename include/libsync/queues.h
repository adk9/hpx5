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


typedef struct {
  queue_t vtable;
  volatile cptr_t head;
  volatile cptr_t tail;
} ms_queue_t;

ms_queue_t *sync_ms_queue_new(void) HPX_MALLOC;
void sync_ms_queue_delete(ms_queue_t *q) HPX_NON_NULL(1);

void  sync_ms_queue_init(ms_queue_t *q) HPX_NON_NULL(1);
void  sync_ms_queue_fini(ms_queue_t *q) HPX_NON_NULL(1);
void  sync_ms_queue_enqueue(ms_queue_t *q, void *val) HPX_NON_NULL(1);
void *sync_ms_queue_dequeue(ms_queue_t *q) HPX_NON_NULL(1);

#endif // LIBHPX_QUEUES_H
