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
#ifndef LIBHPX_MS_QUEUE_H
#define LIBHPX_MS_QUEUE_H

/// ----------------------------------------------------------------------------
/// @file ms_queue.h
/// @brief A simple, macro-based interface to an M&S Queue.
///
/// ----------------------------------------------------------------------------
#include "cptr.h"

typedef struct {
  volatile cptr_t head;
  volatile cptr_t tail;
} ms_queue_t;

void  sync_ms_queue_init(ms_queue_t *q);
void  sync_ms_queue_enqueue(ms_queue_t *q, void *val);
void *sync_ms_queue_dequeue(ms_queue_t *queue);

#define SYNC_MS_QUEUE_INITIALIZER { \
    .head = SYNC_CPTR_INITIALIZER,  \
    .tail = SYNC_CPTR_INITIALIZER   \
    }
#define SYNC_MS_QUEUE_INIT(queue) sync_ms_queue_init(queue)
#define SYNC_MS_QUEUE_ENQUEUE(queue, val) sync_ms_queue_enqueue(queue, val)
#define SYNC_MS_QUEUE_DEQUEUE(queue, val) val = sync_ms_queue_dequeue(queue)

#endif // LIBHPX_MS_QUEUE_H
