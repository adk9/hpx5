/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Binomial Heap Functions
  hpx_heap.c

  Copyright (c) 2008, Bjoern B. Brandenburg
  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  Authors:
    Bjoern B. Brandenburg <bbb [at] cs.unc.edu>
    Patrick K. Bohan <pbohan [at] indiana.edu>

  https://github.com/brandenburg/binomial-heaps/blob/master/heap.h
 ====================================================================
*/

#pragma once
#ifndef LIBHPX_HEAP_H_
#define LIBHPX_HEAP_H_

#include <limits.h>
#include <stdlib.h>

#define HPX_NOT_IN_HEAP UINT_MAX

typedef struct _hpx_heap_node_t {
  struct _hpx_heap_node_t *parent;
  struct _hpx_heap_node_t *next;
  struct _hpx_heap_node_t *child;

  unsigned int degree;
  void *value;

  struct _hpx_heap_node_t **ref;
} hpx_heap_node_t;

typedef struct {
  hpx_heap_node_t *head;

  /* We cache the minimum of the heap. This speeds up repeated peek operations. */
  hpx_heap_node_t *min;
} hpx_heap_t;

/* item comparison function:
 * return 1 if a has higher prio than b, 0 otherwise
 */
typedef int (* hpx_heap_prio_t)(hpx_heap_node_t *a, hpx_heap_node_t *b);

static inline void hpx_heap_init(hpx_heap_t *h)
{
  h->head = NULL;
  h->min = NULL;
}

static inline void hpx_heap_node_init_ref(hpx_heap_node_t **_h, void *value)
{
  hpx_heap_node_t *h = *_h;
  h->parent = NULL;
  h->next = NULL;
  h->child = NULL;
  h->degree = HPX_NOT_IN_HEAP;
  h->value = value;
  h->ref = _h;
}

static inline void hpx_heap_node_init(hpx_heap_node_t *h, void *value)
{
  h->parent = NULL;
  h->next = NULL;
  h->child = NULL;
  h->degree = HPX_NOT_IN_HEAP;
  h->value = value;
  h->ref = NULL;
}

static inline void *hpx_heap_node_value(hpx_heap_node_t *h)
{
  return h->value;
}

static inline int hpx_heap_node_in_heap(hpx_heap_node_t *h)
{
  return h->degree != HPX_NOT_IN_HEAP;
}

static inline int hpx_heap_empty(hpx_heap_t *heap)
{
  return heap->head == NULL && heap->min == NULL;
}

/* make child a subtree of root */
static inline void __hpx_heap_link(hpx_heap_node_t *root,
			           hpx_heap_node_t *child)
{
  child->parent = root;
  child->next = root->child;
  root->child = child;
  root->degree++;
}

/* merge root lists */
static inline hpx_heap_node_t *__hpx_heap_merge(hpx_heap_node_t *a,
                                                hpx_heap_node_t *b)
{
  hpx_heap_node_t *head = NULL;
  hpx_heap_node_t **pos = &head;

  while (a && b) {
    if (a->degree < b->degree) {
      *pos = a;
      a = a->next;
    } else {
      *pos = b;
      b = b->next;
    }
    pos = &(*pos)->next;
  }
  if (a)
    *pos = a;
  else
    *pos = b;
  return head;
}

/* reverse a linked list of nodes. also clears parent pointer */
static inline hpx_heap_node_t *__hpx_heap_reverse(hpx_heap_node_t *h)
{
  hpx_heap_node_t *tail = NULL;
  hpx_heap_node_t *next;

  if (!h)
    return h;

  h->parent = NULL;
  while (h->next) {
    next = h->next;
    h->next = tail;
    tail = h;
    h = next;
    h->parent = NULL;
  }
  h->next = tail;
  return h;
}

static inline void __hpx_heap_min(hpx_heap_prio_t higher_prio, hpx_heap_t *heap,
			          hpx_heap_node_t **prev, hpx_heap_node_t **node)
{
  hpx_heap_node_t *_prev, *cur;
  *prev = NULL;

  if (!heap->head) {
    *node = NULL;
    return;
  }

  *node = heap->head;
  _prev = heap->head;
  cur = heap->head->next;
  while (cur) {
    if (higher_prio(cur, *node)) {
      *node = cur;
      *prev = _prev;
    }
    _prev = cur;
    cur = cur->next;
  }
}

static inline void __hpx_heap_union(hpx_heap_prio_t higher_prio, hpx_heap_t *heap,
				    hpx_heap_node_t *h2)
{
  hpx_heap_node_t *h1;
  hpx_heap_node_t *prev, *x, *next;

  if (!h2)
    return;
  h1 = heap->head;
  if (!h1) {
    heap->head = h2;
    return;
  }
  h1 = __hpx_heap_merge(h1, h2);
  prev = NULL;
  x = h1;
  next = x->next;
  while (next) {
    if (x->degree != next->degree ||
	(next->next && next->next->degree == x->degree)) {
      /* nothing to do, advance */
      prev = x;
      x = next;
    } else if (higher_prio(x, next)) {
      /* x becomes the root of next */
      x->next = next->next;
      __hpx_heap_link(x, next);
    } else {
      /* next becomes the root of x */
      if (prev)
	prev->next = next;
      else
	h1 = next;
      __hpx_heap_link(next, x);
      x = next;
    }
    next = x->next;
  }
  heap->head = h1;
}

static inline hpx_heap_node_t *__hpx_heap_extract_min(hpx_heap_prio_t higher_prio,
                                                      hpx_heap_t *heap)
{
  hpx_heap_node_t *prev, *node;
  __hpx_heap_min(higher_prio, heap, &prev, &node);
  if (!node)
    return NULL;
  if (prev)
    prev->next = node->next;
  else
    heap->head = node->next;
  __hpx_heap_union(higher_prio, heap, __hpx_heap_reverse(node->child));
  return node;
}

/* insert (and reinitialize) a node into the heap */
static inline void hpx_heap_insert(hpx_heap_prio_t higher_prio, hpx_heap_t *heap,
			           hpx_heap_node_t *node)
{
  hpx_heap_node_t *min;
  node->child = NULL;
  node->parent = NULL;
  node->next = NULL;
  node->degree = 0;
  if (heap->min && higher_prio(node, heap->min)) {
    /* swap min cache */
    min = heap->min;
    min->child = NULL;
    min->parent = NULL;
    min->next = NULL;
    min->degree = 0;
    __hpx_heap_union(higher_prio, heap, min);
    heap->min = node;
  } else
    __hpx_heap_union(higher_prio, heap, node);
}

static inline void __hpx_uncache_min(hpx_heap_prio_t higher_prio, hpx_heap_t *heap)
{
  hpx_heap_node_t *min;
  if (heap->min) {
    min = heap->min;
    heap->min = NULL;
    hpx_heap_insert(higher_prio, heap, min);
  }
}

/* merge addition into target */
static inline void hpx_heap_union(hpx_heap_prio_t higher_prio,
			          hpx_heap_t * target, hpx_heap_t *addition)
{
  /* first insert any cached minima, if necessary */
  __hpx_uncache_min(higher_prio, target);
  __hpx_uncache_min(higher_prio, addition);
  __hpx_heap_union(higher_prio, target, addition->head);
  /* this is a destructive merge */
  addition->head = NULL;
}

static inline hpx_heap_node_t *hpx_heap_peek(hpx_heap_prio_t higher_prio,
                                             hpx_heap_t *heap)
{
  if (!heap->min)
    heap->min = __hpx_heap_extract_min(higher_prio, heap);
  return heap->min;
}

static inline hpx_heap_node_t *heap_take(hpx_heap_prio_t higher_prio,
                                         hpx_heap_t * heap)
{
  hpx_heap_node_t *node;
  if (!heap->min)
    heap->min = __hpx_heap_extract_min(higher_prio, heap);
  node = heap->min;
  heap->min = NULL;
  if (node)
    node->degree = HPX_NOT_IN_HEAP;
  return node;
}

static inline void hpx_heap_decrease(hpx_heap_prio_t higher_prio, hpx_heap_t *heap,
				     hpx_heap_node_t *node)
{
  hpx_heap_node_t *parent;
  hpx_heap_node_t **tmp_ref;
  void *tmp;

  /* node's priority was decreased, we need to update its position */
  if (!node->ref)
    return;
  if (heap->min != node) {
    if (heap->min && higher_prio(node, heap->min))
      __hpx_uncache_min(higher_prio, heap);
    /* bubble up */
    parent = node->parent;
    while (parent && higher_prio(node, parent)) {
      /* swap parent and node */
      tmp = parent->value;
      parent->value = node->value;
      node->value = tmp;
      /* swap references */
      if (parent->ref)
	*(parent->ref) = node;
      *(node->ref) = parent;
      tmp_ref = parent->ref;
      parent->ref = node->ref;
      node->ref = tmp_ref;
      /* step up */
      node = parent;
      parent = node->parent;
    }
  }
}

static inline void hpx_heap_delete(hpx_heap_prio_t higher_prio, hpx_heap_t *heap,
			           hpx_heap_node_t *node)
{
  hpx_heap_node_t *parent, *prev, *pos;
  hpx_heap_node_t **tmp_ref;
  void *tmp;

  if (!node->ref) /* can only delete if we have a reference */
    return;
  if (heap->min != node) {
    /* bubble up */
    parent = node->parent;
    while (parent) {
      /* swap parent and node */
      tmp = parent->value;
      parent->value = node->value;
      node->value = tmp;
      /* swap references */
      if (parent->ref)
	*(parent->ref) = node;
      *(node->ref) = parent;
      tmp_ref = parent->ref;
      parent->ref = node->ref;
      node->ref = tmp_ref;
      /* step up */
      node = parent;
      parent = node->parent;
    }
    /* now delete:
     * first find prev */
    prev = NULL;
    pos = heap->head;
    while (pos != node) {
      prev = pos;
      pos = pos->next;
    }
    /* we have prev, now remove node */
    if (prev)
      prev->next = node->next;
    else
      heap->head = node->next;
    __hpx_heap_union(higher_prio, heap, __hpx_heap_reverse(node->child));
  } else
    heap->min = NULL;
  node->degree = HPX_NOT_IN_HEAP;
}

#endif /* LIBHPX_HEAP_H */
