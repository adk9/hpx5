
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Linked List Function Definitions
  hpx_list.c

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  Authors:
    Patrick K. Bohan <pbohan [at] indiana.edu>

  https://github.com/brandenburg/binomial-heaps/blob/master/heap.h
 ====================================================================
*/

#pragma once
#ifndef LIBHPX_LIST_H_
#define LIBHPX_LIST_H_

#include <stdint.h>
#include <stdlib.h>
#include "hpx_mem.h"


/*
 --------------------------------------------------------------------
  List Data Structures
 --------------------------------------------------------------------
*/

typedef struct _hpx_list_node_t {
  struct _hpx_list_node_t * next;
  void * value;
} hpx_list_node_t;

typedef struct _hpx_list_t {
  struct _hpx_list_node_t * head;
  uint64_t count;
} hpx_list_t;
		   

/*
 --------------------------------------------------------------------
  List Functions
 --------------------------------------------------------------------
*/

void hpx_list_init(hpx_list_t *);
uint64_t hpx_list_size(hpx_list_t *);
void * hpx_list_peek(hpx_list_t *); 
void hpx_list_push(hpx_list_t *, void *);
void * hpx_list_pop(hpx_list_t *);
void hpx_list_delete(hpx_list_t *, void *);

hpx_list_node_t * hpx_list_first(hpx_list_t *);
hpx_list_node_t * hpx_list_next(hpx_list_node_t *);


/*
 --------------------------------------------------------------------
  hpx_list_init

  Initialize a linked list.  This function should be called
  before using any other functions on this queue.
 --------------------------------------------------------------------
*/

inline void hpx_list_init(hpx_list_t * ll) {  
  ll->head = NULL;
  ll->count = 0;
} __attribute((always_inline));


/*
 --------------------------------------------------------------------
  hpx_list_destroy

  Frees any memory allocated by this list.  Should be called after
  all other functions.
 --------------------------------------------------------------------
*/

inline void hpx_list_destroy(hpx_list_t * ll) {
  hpx_list_node_t * cur = NULL;
  hpx_list_node_t * next = NULL;

  cur = ll->head;
  while (cur != NULL) {
    next = cur->next;
    hpx_free(cur);
    cur = next;
  }

  ll->head = NULL;
  ll->count = 0;
} __attribute__((always_inline));


/*
 --------------------------------------------------------------------
  hpx_list_size

  Returns the number of elements in the list.
 --------------------------------------------------------------------
*/

inline uint64_t hpx_list_size(hpx_list_t * ll) {
  return ll->count;
} __attribute__((always_inline));


/*
 --------------------------------------------------------------------
  hpx_list_peek

  Returns the last element WITHOUT popping it off of the list.
 --------------------------------------------------------------------
*/

inline void * hpx_list_peek(hpx_list_t * ll) {
  hpx_list_node_t * node;
  void * val = NULL;

  node = ll->head;
  if (node != NULL) {
    val = node->value;
  }
  
  return val;
} __attribute__((always_inline));


/*
 --------------------------------------------------------------------
  hpx_list_push

  Push an element into the end of the list.
 --------------------------------------------------------------------
*/

inline void hpx_list_push(hpx_list_t * ll, void * val) {
  hpx_list_node_t * node = NULL;

  node = (hpx_list_node_t *) hpx_alloc(sizeof(hpx_list_node_t));
  if (node != NULL) {
    node->value = val;
    node->next = ll->head;

    ll->head = node;
    ll->count += 1;
  }
} __attribute__((always_inline));


/*
 --------------------------------------------------------------------
  hpx_list_pop

  Pops the last element off of the list and returns it.
 --------------------------------------------------------------------
*/

inline void * hpx_list_pop(hpx_list_t * ll) {
  hpx_list_node_t * node = NULL;
  void * val = NULL;

  node = ll->head;
  if (node != NULL) {
    val = node->value;

    ll->head = node->next;
    hpx_free(node);

    ll->count -= 1;
  }

  return val;
} __attribute__((always_inline));


/*
 --------------------------------------------------------------------
  hpx_list_first

  Returns the first node in the list.
 --------------------------------------------------------------------
*/

inline hpx_list_node_t * hpx_list_first(hpx_list_t * ll) {
  return ll->head;
} __attribute__((always_inline));


/*
 --------------------------------------------------------------------
  hpx_list_next

  Returns the next element in the list.
 --------------------------------------------------------------------
*/

inline hpx_list_node_t * hpx_list_next(hpx_list_node_t * node) {
  return node->next;
} __attribute__((always_inline));


/*
 --------------------------------------------------------------------
  hpx_list_delete

  Deletes an element from the list (by value).
 --------------------------------------------------------------------
*/

inline void hpx_list_delete(hpx_list_t * ll, void * val) {
  struct _hpx_list_node_t * found = NULL;
  struct _hpx_list_node_t * prev = NULL;
  struct _hpx_list_node_t * cur = NULL;

  cur = ll->head;
  prev = NULL;

  while ((cur != NULL) && (found == NULL)) {
    if (cur->value == val) {
      found = cur;
    } else {
      prev = cur;
      cur = cur->next;
    }
  }

  if (found != NULL) {
    if (prev == NULL) {
      hpx_list_pop(ll);
    } else {
      prev->next = found->next;
      hpx_free(found);
    }

    ll->count -= 1;
  }
} __attribute__((always_inline));

#endif
