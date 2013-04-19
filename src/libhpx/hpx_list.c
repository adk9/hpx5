
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Linked List Functions
  hpx_list.c

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
#include "hpx_list.h"
#include "hpx_mem.h"


/*
 --------------------------------------------------------------------
  hpx_list_init

  Initialize a linked list.  This function should be called
  before using any other functions on this queue.
 --------------------------------------------------------------------
*/

void hpx_list_init(hpx_list_t * ll) {  
  SLIST_INIT(&ll->head);
  ll->count = 0;
}


/*
 --------------------------------------------------------------------
  hpx_list_destroy

  Frees any memory allocated by this list.  Should be called after
  all other functions.
 --------------------------------------------------------------------
*/

void hpx_list_destroy(hpx_list_t * ll) {
  hpx_list_node_t * cur = NULL;
  hpx_list_node_t * next = NULL;

  cur = SLIST_FIRST(&ll->head);
  while (cur != NULL) {
    next = SLIST_NEXT(cur, entries);
    hpx_free(cur);
    cur = next;
  }

  SLIST_INIT(&ll->head);
  ll->count = 0;
}


/*
 --------------------------------------------------------------------
  hpx_list_size

  Returns the number of elements in the list.
 --------------------------------------------------------------------
*/

uint64_t hpx_list_size(hpx_list_t * ll) {
  return ll->count;
}


/*
 --------------------------------------------------------------------
  hpx_list_peek

  Returns the last element WITHOUT popping it off of the list.
 --------------------------------------------------------------------
*/

void * hpx_list_peek(hpx_list_t * ll) {
  hpx_list_node_t * node;
  void * val = NULL;
  
  node = SLIST_FIRST(&ll->head);
  if (node != NULL) {
    val = node->value;
  }
  
  return val;
}


/*
 --------------------------------------------------------------------
  hpx_list_push

  Push an element into the end of the list.
 --------------------------------------------------------------------
*/

void hpx_list_push(hpx_list_t * ll, void * val) {
  hpx_list_node_t * node = NULL;

  node = (hpx_list_node_t *) hpx_alloc(sizeof(hpx_list_node_t));
  if (node != NULL) {
    node->value = val;

    SLIST_INSERT_HEAD(&ll->head, node, entries);
    ll->count += 1;
  }
}


/*
 --------------------------------------------------------------------
  hpx_list_pop

  Pops the last element off of the list and returns it.
 --------------------------------------------------------------------
*/

void * hpx_list_pop(hpx_list_t * ll) {
  hpx_list_node_t * node = NULL;
  void * val = NULL;

  node = SLIST_FIRST(&ll->head);
  if (node != NULL) {
    val = node->value;
    SLIST_REMOVE_HEAD(&ll->head, entries);
    hpx_free(node);
  }

  ll->count -= 1;

  return val;
}
