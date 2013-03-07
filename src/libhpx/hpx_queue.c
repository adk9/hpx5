
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Priority Queue Functions
  hpx_queue.c

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).

  Authors:
    Patrick K. Bohan <pbohan [at] indiana.edu>
 ====================================================================
*/


#include <stdlib.h>
#include "hpx_queue.h"
#include "hpx_mem.h"


/*
 --------------------------------------------------------------------
  hpx_queue_init

  Initialize a priority queue.  This function should be called
  before using any other functions on this queue.
 --------------------------------------------------------------------
*/

void hpx_queue_init(hpx_queue_t * q) {
  q->head = NULL;
  q->tail = NULL;
  q->count = 0;
}


/*
 --------------------------------------------------------------------
  hpx_queue_destroy

  Frees any memory allocated by this queue.  Should be called after
  all other functions.
 --------------------------------------------------------------------
*/

void hpx_queue_destroy(hpx_queue_t * q) {
  hpx_queue_node_t * next = NULL;
  hpx_queue_node_t * cur = NULL;

  cur = q->head;
  while (cur != NULL) {
    next = cur->next;
    hpx_free(cur);
    cur = next;
  }
}


/*
 --------------------------------------------------------------------
  hpx_queue_size

  Returns the number of elements in the queue.
 --------------------------------------------------------------------
*/

uint64_t hpx_queue_size(hpx_queue_t * q) {
  return q->count;
}


/*
 --------------------------------------------------------------------
  hpx_queue_peek

  Returns the front element WITHOUT popping it off of the queue.
 --------------------------------------------------------------------
*/

void * hpx_queue_peek(hpx_queue_t * q) {
  void * val = NULL;
  
  if (q->tail != NULL) {
    val = q->tail->value;
  }
  
  return val;
}


/*
 --------------------------------------------------------------------
  hpx_queue_push

  Push an element into the back of the queue.
 --------------------------------------------------------------------
*/

void hpx_queue_push(hpx_queue_t * q, void * val) {
  hpx_queue_node_t * node = NULL;
  hpx_queue_node_t * head = NULL;

  /* allocate the node */
  node = (hpx_queue_node_t *) hpx_alloc(sizeof(hpx_queue_node_t));
  if (node != NULL) {
    /* set up the node */
    node->prev = NULL;
    node->next = q->head;
    node->value = val;

    /* link in the current head */
    head = q->head;
    if (head != NULL) {
      head->prev = node;
    }

    /* set the queue's head */
    q->head = node;

    /* set the queue's tail if this is the only element */
    if (q->tail == NULL) {
      q->tail = node;
    }

    /* increment the element counter */
    q->count += 1;
  }
}


/*
 --------------------------------------------------------------------
  hpx_queue_pop

  Pops the front element off of the queue and returns it.
 --------------------------------------------------------------------
*/

void * hpx_queue_pop(hpx_queue_t * q) {
  hpx_queue_node_t * node = NULL;
  hpx_queue_node_t * prev = NULL;
  void * val = NULL;

  if (q->tail != NULL) {
    /* detach the tail */
    node = q->tail;

    prev = node->prev;
    q->tail = prev;
    if (prev != NULL) {
      prev->next = NULL;
    } else {
      q->head = NULL;
    }
    
    /* set the value to return and decrement the element counter */
    val = node->value;
    q->count -= 1;

    /* free the node */
    hpx_free(node);
    node = NULL;
  }

  return val;
}
