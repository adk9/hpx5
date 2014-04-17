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
#include "config.h"
#endif

#include <stddef.h>
#include <stdlib.h>

#include "hpx/builtins.h"
#include "libsync/queues.h"
#include "backoff.h"

struct two_lock_queue_node {
  two_lock_queue_node_t *next;
  void *value;
};

static __thread two_lock_queue_node_t *_free = NULL;

static two_lock_queue_node_t *_node_new(void *value) {
  two_lock_queue_node_t *node = _free;
  if (node) {
    _free = node->next;
  }
  else {
    node = malloc(sizeof(*node));
  }
  node->next = NULL;
  node->value = value;
  return node;
}


static void _node_delete(two_lock_queue_node_t *node) {
  node->next = _free;
  _free = node;
}


static two_lock_queue_node_t *_acquire(two_lock_queue_node_t **ptr) {
  static const int base = 16;
  int i = base;
  two_lock_queue_node_t *node = sync_swap(ptr, (void*)1, SYNC_ACQUIRE);

  while (unlikely(node == (void*)1)) {
    backoff(&i);
    node = sync_swap(ptr, (void*)1, SYNC_ACQUIRE);
  }

  return node;
}


void sync_two_lock_queue_init(two_lock_queue_t *q) {
  q->vtable.delete  = (__typeof__(q->vtable.delete))sync_two_lock_queue_delete;
  q->vtable.enqueue = (__typeof__(q->vtable.enqueue))sync_two_lock_queue_enqueue;
  q->vtable.dequeue = (__typeof__(q->vtable.dequeue))sync_two_lock_queue_dequeue;

  q->head = q->tail = _node_new(NULL);
}

two_lock_queue_t *sync_two_lock_queue_new(void) {
  two_lock_queue_t *q;
  int e = posix_memalign((void**)&q, HPX_CACHELINE_SIZE, sizeof(*q));
  if (e == 0)
    sync_two_lock_queue_init(q);
  return q;
}

void sync_two_lock_queue_fini(two_lock_queue_t *q) {
  while (sync_two_lock_queue_dequeue(q))
    ;
  _node_delete(q->head);
}


void sync_two_lock_queue_delete(two_lock_queue_t *q) {
  if (!q)
    return;
  sync_two_lock_queue_fini(q);
  free(q);
}


void sync_two_lock_queue_enqueue(two_lock_queue_t *q, void *val) {
  two_lock_queue_node_t *node = _node_new(val);
  two_lock_queue_node_t *tail = _acquire(&q->tail);
  tail->next = node;
  sync_store(&q->tail, node, SYNC_RELEASE);
}


void *sync_two_lock_queue_dequeue(two_lock_queue_t *q) {
  two_lock_queue_node_t *head = _acquire(&q->head);
  two_lock_queue_node_t *next = head->next;
  if (next == NULL) {
    sync_store(&q->head, head, SYNC_RELEASE);
    return NULL;
  }

  void *value = next->value;
  sync_store(&q->head, next, SYNC_RELEASE);
  _node_delete(head);
  return value;
}
