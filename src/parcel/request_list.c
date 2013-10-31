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
#if DEBUG
#include <stdio.h>
#include "serialization.h"
#endif

void request_list_init(struct request_list *list) {
  list->head = NULL;
  list->tail = NULL;
  list->prev = NULL;
  list->curr = NULL;
  list->size = 0;
}

/* Perhaps confusingly, this function returns a struct network_request* so
   that it can be used in the get() call. It's done this way so we can
   avoid an extra alloc() we really don't need. */
struct network_request* request_list_append(struct request_list *list,
                                            struct header *parcel) {
  struct request_list_node* node = hpx_alloc(sizeof(*node));
  if (!node) {
    __hpx_errno = HPX_ERROR_NOMEM;
    return NULL;
  }    
#if 0
  node->parcel = parcel;
  node->next = list->head;
  list->head = node;
  if (list->tail == NULL)
    list->tail = node;
#endif
  //
  node->parcel = parcel;
  node->next = NULL;
  if (list->head == NULL)
    list->head = node;
  if (list->tail != NULL)
    list->tail->next = node;
  list->tail = node;
  //

  list->size++;

#if DEBUG
  printf("%d: Request list at %p appended parcel at %p with id %d; new size is %d\n", hpx_get_rank(), (void*)list, (void*)parcel, parcel->parcel_id, list->size);
  fflush(stdout);
#endif

  return &node->request;
}

void request_list_begin(struct request_list *list) {
  list->prev = NULL;
  list->curr = list->head;

#if DEBUG
  if (list->curr != NULL)
    printf("%d: -- Request list at %p begin() == %p with id %d; size is %d\n", hpx_get_rank(), (void*)list, (void*)list->curr->parcel, list->curr->parcel->parcel_id, list->size);
  else
    printf("%d: -- Request list at %p begin() == NULL;          size is %d\n", hpx_get_rank(), (void*)list, list->size);
  fflush(stdout);
#endif

} 

struct network_request* request_list_curr(struct request_list *list) {
  return (list->curr) ? &list->curr->request : NULL;
} 

struct header* request_list_curr_parcel(struct request_list *list) {
  return (list->curr) ? list->curr->parcel : NULL;
} 

void request_list_next(struct request_list *list) {
  if (list->curr != NULL) {
    list->prev = list->curr;
    list->curr = list->curr->next;
  }
  else if (list->size > 0) {
    list->prev = NULL;
    list->curr = list->head;
    }

#if DEBUG
  if (list->curr != NULL)
    printf("%d: -- Request list at %p next() == %p with id %d; size is %d\n", hpx_get_rank(), (void*)list, (void*)list->curr->parcel, list->curr->parcel->parcel_id, list->size);
  else
    printf("%d: -- Request list at %p next() == NULL;          size is %d\n", hpx_get_rank(), (void*)list, list->size);
  fflush(stdout);
#endif

}

void request_list_del(struct request_list *list) {
  struct request_list_node* node = list->curr;
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
  list->curr = list->prev; /* do this instead of making it next since we want next() to still work in a for loop if we delete */
  list->size--;
#if DEBUG
  printf("%d: Request list at %p deleted parcel at %p with id %d; new size is %d\n", hpx_get_rank(), (void*)list, (void*)node->parcel, node->parcel->parcel_id, list->size);
  fflush(stdout);
#endif
  hpx_free(node);
}
