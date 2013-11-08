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

#include <assert.h>
#include <stdbool.h>
#include "parcelqueue.h"
#include "serialization.h"
#include "hpx/mem.h"                            /* hpx_{alloc,free} */
#include "hpx/error.h"                          /* __hpx_errno */

/*
  ====================================================================
  This implements the parcel queue.

  LD: This is basically broken for multithreaded use. It needs proper
  synchronization so that it doesn't have any data races. It won't
  be too hard to fix, but I don't have time at the moment.
  Basically, we just need to implement the M&S queue for pushers,
  and properly synchronize the trypop read operations.
  ====================================================================
*/

struct pq_node {
  struct pq_node* next;
  void* value;
};

int parcelqueue_create(struct parcelqueue** q_handle) {
  struct parcelqueue *q = *q_handle;

  /* allocate a queue */
  q = hpx_alloc(sizeof(*q));
  if (q == NULL)
    return (__hpx_errno = HPX_ERROR_NOMEM);

  /* allocate the dummy node */
  q->head = hpx_alloc(sizeof(*q->head));
  if (q->head == NULL) {
    hpx_free(q);
    return (__hpx_errno = HPX_ERROR_NOMEM);
  }

  /* initialize the fields */
  q->head->next = NULL;
  q->tail = q->head;
  q->head_lock = hpx_lco_mutex_create(0);
  q->tail_lock = hpx_lco_mutex_create(0);
  *q_handle = q;
  return HPX_SUCCESS;
}

void* parcelqueue_trypop(struct parcelqueue* q) {
  /* precondition, q != NULL */
  if (q == NULL) {
    __hpx_errno = HPX_ERROR;                    /*TODO: more specific error */
    return NULL;
  }
  
  hpx_lco_mutex_lock(q->head_lock);
  struct pq_node *node = q->head;
  struct pq_node *next = node->next; 
  if (next == NULL) {                       
    hpx_lco_mutex_unlock(q->head_lock);
    return NULL;                     
  }  

  void* val = next->value;
  q->head = next;                               /* LD: atomic? */
  hpx_lco_mutex_unlock(q->head_lock);
  hpx_free(node);
  return val;
}

int parcelqueue_push(struct parcelqueue* q, void* val) {
  /* precondition, q != NULL */
  if (q == NULL)
    return (__hpx_errno = HPX_ERROR);           /*TODO: more specific error */

  struct pq_node *node = hpx_alloc(sizeof(*node));
  if (node == NULL) 
    return (__hpx_errno = HPX_ERROR_NOMEM);
  
  node->next = NULL;
  node->value = val;

  /* LD: this only works because there is no "pop," only "trypop," and
     even given that I'm not sure that it's safe. It's certainly no properly
     synchronized and will be undefined on C11. Also, there's no reason to use
     a lock here we can just CAS the node in.

     TODO: FIX THIS!
  */
  /* CRITICAL SECTION */
  hpx_lco_mutex_lock(q->tail_lock);

  q->tail->next = node;
  q->tail = node;

  hpx_lco_mutex_unlock(q->tail_lock);
  /* END CRITICAL SECTION */
  return HPX_SUCCESS;
}

int parcelqueue_destroy(struct parcelqueue** q_handle) {
  struct parcelqueue* q = *q_handle;

  /* precondition, *q_handle != NULL */
  /* LD: why do we care? */
  if (q == NULL)
    return (__hpx_errno = HPX_ERROR);           /* TODO: more specific error */

  while (parcelqueue_trypop(q))
    /* empty on purpose*/;

  assert(q->head && "Expected sentinel node");
  assert(q->head == q->tail && "Expected sentinel node");

  hpx_lco_mutex_destroy(q->head_lock);
  hpx_lco_mutex_destroy(q->tail_lock);
  hpx_free(q->head);
  hpx_free(q);
  q = NULL;
  return HPX_SUCCESS;
}

bool parcelqueue_empty(struct parcelqueue* q) {
  if (q == NULL) {
    __hpx_errno = HPX_ERROR;                    /*TODO: more specific error */
    return NULL;
  }
  
  hpx_lco_mutex_lock(q->head_lock);
  struct pq_node *next = q->head->next;
  hpx_lco_mutex_unlock(q->head_lock);
  if (next == NULL)
    return true;                     
  else
    return false;
}
