
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
#include <sys/queue.h>
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
  STAILQ_INIT(&q->head);
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
  hpx_queue_node_t * cur = NULL;
  hpx_queue_node_t * next = NULL;

  cur = STAILQ_FIRST(&q->head);
  while (cur != NULL) {
    next = STAILQ_NEXT(cur, entries);
    hpx_free(cur);
    cur = next;
  }

  STAILQ_INIT(&q->head);
  q->count = 0;
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
  hpx_queue_node_t * node;
  void * val = NULL;
  
  node = STAILQ_FIRST(&q->head);
  if (node != NULL) {
    val = node->value;
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

  node = (hpx_queue_node_t *) hpx_alloc(sizeof(hpx_queue_node_t));
  if (node != NULL) {
    node->value = val;

    STAILQ_INSERT_TAIL(&q->head, node, entries);
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
  void * val = NULL;

  node = STAILQ_FIRST(&q->head);
  if (node != NULL) {
    val = node->value;
    STAILQ_REMOVE_HEAD(&q->head, entries);
    hpx_free(node);
  }

  q->count -= 1;

  return val;
}
