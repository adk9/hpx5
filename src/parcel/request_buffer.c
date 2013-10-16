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
#include "request_buffer.h"
#include "hpx/error.h"                          /* __hpx_errno HPX_* */
#include "hpx/mem.h"                            /* hpx_{alloc,free} */
#include "network/network.h"                    /* struct request_buffer */

int request_buffer_init(struct request_buffer* q, size_t limit) {

  q->requests = hpx_alloc(sizeof(*q->requests) * limit); 
  if (q->requests == NULL)
    return (__hpx_errno = HPX_ERROR_NOMEM);

  q->head = 0;
  q->size = 0;
  q->limit = limit;  
  return HPX_SUCCESS;
}

int request_buffer_destroy(struct request_buffer* q) {
  hpx_free(q->requests);
  return HPX_SUCCESS;
}

bool request_buffer_full(struct request_buffer* q) {
  return !(q != NULL && q->size < q->limit);
}

bool request_buffer_empty(struct request_buffer* q) {
  return !(q != NULL && q->size > 0);
}

/* caller must check requeue_queue_full first */
network_request_t* request_buffer_push(struct request_buffer* q) {
  network_request_t* ret = NULL;
  if (!request_buffer_full(q)) {
    ret = &(q->requests[(q->head + q->size) % q->limit]);
    q->size++;
  }
  return ret;
}

network_request_t* request_buffer_head(struct request_buffer* q) {
  return &(q->requests[q->head]);
}

void request_buffer_pop(struct request_buffer* q) {
  if (!request_buffer_empty(q))
    q->head++;
  if (q->head == q->limit) /* wrap around */
    q->head = 0;
  q->size--;
}

