/*
 ====================================================================
  Linked List Function Definitions
  list.c

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  Authors:
    Patrick K. Bohan <pbohan [at] indiana.edu>
    Benjamin D. Martin <benjmart [at] indiana.edu>
 ====================================================================
*/

#pragma once
#ifndef LIST_H_
#define LIST_H_

#include <stdint.h>
#include <stdlib.h>

/*
 --------------------------------------------------------------------
  List Data Structures
 --------------------------------------------------------------------
*/

typedef struct list_node_t {
  struct list_node_t *next;
  void *value;
} list_node_t __attribute__((aligned (8)));

typedef struct list_t {
  struct list_node_t *head;
  struct list_node_t *tail;
  uint64_t count;
} list_t __attribute__((aligned (8)));
		   

/*
 --------------------------------------------------------------------
  List Functions
 --------------------------------------------------------------------
*/

/*
 --------------------------------------------------------------------
  list_init

  Initialize a linked list.  This function should be called
  before using any other functions on this queue.
 --------------------------------------------------------------------
*/
static inline void list_init(list_t *ll) {  
  ll->head = NULL;
  ll->tail = NULL;
  ll->count = 0;
}


/*
 --------------------------------------------------------------------
  list_destroy

  Frees any memory allocated by this list.  Should be called after
  all other functions.
 --------------------------------------------------------------------
*/
static inline void list_destroy(list_t *ll) {
  list_node_t *cur = NULL;
  list_node_t *next = NULL;

  cur = ll->head;
  while (cur != NULL) {
    next = cur->next;
    free(cur);
    cur = next;
  }

  ll->head = NULL;
  ll->tail = NULL;
  ll->count = 0;
}


/*
 --------------------------------------------------------------------
  list_size

  Returns the number of elements in the list.
 --------------------------------------------------------------------
*/
static inline uint64_t list_size(list_t *ll) {
  return ll->count;
}


/*
 --------------------------------------------------------------------
  list_peek

  Returns the last element WITHOUT popping it off of the list.
 --------------------------------------------------------------------
*/
static inline void *list_peek(list_t *ll) {
  list_node_t *node;
  void * val = NULL;

  node = ll->head;
  if (node != NULL) {
    val = node->value;
  }
  
  return val;
} 


/*
 --------------------------------------------------------------------
  list_push_front

  Push an element into the end of the list.
 --------------------------------------------------------------------
*/
static inline void list_push_front(list_t *ll, void *val) {
  list_node_t *node = NULL;

  node = (list_node_t *) malloc(sizeof(list_node_t));
  if (node != NULL) {
    node->value = val;
    node->next = ll->head;

    ll->head = node;
    if (ll->count == 0)
      ll->tail = node;
    ll->count += 1;
  }
}


/*
 --------------------------------------------------------------------
  list_push_back

  Push an element into the back of the queue.
 --------------------------------------------------------------------
*/
static inline void list_push_back(list_t *ll, void *val) {
  list_node_t *node = NULL;

  node = (struct list_node_t *) malloc(sizeof(*node));
  if (node != NULL) {
    node->value = val;
    node->next = NULL;

    if (ll->count == 0) {
      ll->head = node;
    } else {
      ll->tail->next = node;
    }

    ll->tail = node;
    ll->count += 1;
  }
}


/*
 --------------------------------------------------------------------
  list_pop

  Pops the last element off of the list and returns it.
 --------------------------------------------------------------------
*/
static inline void *list_pop(list_t *ll) {
  list_node_t *node = NULL;
  void *val = NULL;

  node = ll->head;
  if (node != NULL) {
    val = node->value;

    ll->head = node->next;
    free(node);

    ll->count -= 1;
    if (ll->count == 0)
      ll->tail = NULL;

  }

  return val;
} 


/*
 --------------------------------------------------------------------
  list_first

  Returns the first node in the list.
 --------------------------------------------------------------------
*/
static inline list_node_t *list_first(list_t *ll) {
  return ll->head;
} 


/*
 --------------------------------------------------------------------
  list_next

  Returns the next element in the list.
 --------------------------------------------------------------------
*/
static inline list_node_t *list_next(list_node_t *node) {
  return node->next;
}


/*
 --------------------------------------------------------------------
  list_delete

  Deletes an element from the list (by value).
 --------------------------------------------------------------------
*/
static inline void list_delete(list_t *ll, void *val) {
  struct list_node_t *found = NULL;
  struct list_node_t *prev = NULL;
  struct list_node_t *cur = NULL;

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
      list_pop(ll);
    } else {
      prev->next = found->next;
      free(found);
      ll->count -= 1;
    }
  }
}

/*
 --------------------------------------------------------------------
  list_insert_after

  Inserts a node after another node, based on the value of the node
 --------------------------------------------------------------------
*/
static inline void list_insert_after(list_t *ll, void *val_after, void* val) {
  struct list_node_t *found = NULL;
  struct list_node_t *prev = NULL;
  struct list_node_t *cur = NULL;

  cur = ll->head;
  prev = NULL;

  while ((cur != NULL) && (found == NULL)) {
    if (cur->value == val_after) {
      found = cur;
    } else {
      prev = cur;
      cur = cur->next;
    }
  }

  if (found != NULL) {
      list_node_t *node = malloc(sizeof(list_node_t));
      if (node != NULL) {
	node->value = val;
	node->next = found->next;
	found->next = node;
	if (node->next == NULL) // i.e. this is the last node
	  ll->tail = node;
	ll->count += 1;
    }
  }
}

#endif /* LIST_H_ */
