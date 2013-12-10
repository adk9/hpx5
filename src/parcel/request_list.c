/*
  ====================================================================
  High Performance ParalleX Library (libhpx)
  
  ParcelQueue Functions
  src/parcel/parcelqueue.c

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).

  Authors:
  Benjamin D. Martin <benjmart [at] indiana.edu>
  Luke Dalessandro   <ldalessa [at] indiana.edu>
  ====================================================================
*/

#include "request_list.h"
#include "hpx/error.h"                          /* __hpx_errno, HPX_* */
#include "hpx/mem.h"                            /* hpx_{alloc,free} */
#include "network.h"                            /* network_request_t */

/* !order matters for this struct!
   We depend on the fact that the request_list_node ISA request. */
struct request_list_node {
  network_request_t      request;               /* must be 1st */
  struct header          *parcel;
  size_t                 size;
  struct request_list_node *next;
};

void
request_list_init(request_list_t *list)
{
  list->head = NULL;
  list->tail = NULL;
  list->prev = NULL;
  list->curr = NULL;
  list->size = 0;
}

/* Perhaps confusingly, this function returns a struct network_request* so
   that it can be used in the get() call. It's done this way so we can
   avoid an extra alloc() we really don't need. */
struct network_request*
request_list_append(request_list_t *list, struct header *parcel)
{
  request_list_node_t* node = hpx_alloc(sizeof(*node));
  if (!node) {
    __hpx_errno = HPX_ERROR_NOMEM;
    return NULL;
  }    
  node->parcel = parcel;
  node->next = NULL;
  if (list->head == NULL)
    list->head = node;
  if (list->tail != NULL)
    list->tail->next = node;
  list->tail = node;

  list->size++;

  return &node->request;
}

void
request_list_begin(request_list_t *list)
{
  list->prev = NULL;
  list->curr = list->head;
} 

struct network_request*
request_list_curr(request_list_t *list)
{
  return (list->curr) ? &list->curr->request : NULL;
} 

struct header*
request_list_curr_parcel(request_list_t *list)
{
  return (list->curr) ? list->curr->parcel : NULL;
} 

size_t
request_list_curr_size(request_list_t *list)
{
  return (list->curr) ? list->curr->size : 0;
} 

void
request_list_next(request_list_t *list)
{
  if (list->curr != NULL) {
    list->prev = list->curr;
    list->curr = list->curr->next;
  }
  else if (list->size > 0) {
    list->prev = NULL;
    list->curr = list->head;
    }
}

void
request_list_del(request_list_t *list)
{
  request_list_node_t* node = list->curr;
  if (!node)
    return;
  
  /* take care of head and tail if necessary */
  if (node == list->head) /* if curr is head, change head to next */
    list->head = list->head->next;
  if (node == list->tail) /* if curr is tail, change tail to prev */
    list->tail = list->prev;

  /* now fix up curr and prev */
  if (list->prev != NULL)
    list->prev->next = node->next; 
  list->curr = list->prev; /* do this instead of making it next since we want
                              next() to still work in a for loop if we delete */ 
  list->size--;
  hpx_free(node);
}
