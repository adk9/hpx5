/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Parcel Handler Function Definitions
  hpx_parcelhandler.h

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of 
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).

  Authors:
    Benjamin D. Martin <benjmart [at] indiana.edu>
 ====================================================================
*/

#pragma once
#ifndef LIBHPX_PARCEL_REQUEST_BUFFER_H_
#define LIBHPX_PARCEL_REQUEST_BUFFER_H_

#include <stddef.h>                             /* size_t */
#include <stdbool.h>                            /* bool */

struct network_request;

/**
   Private storage for network requests. Request queues are ring
buffers and are not intended to be used concurrently.
  */
struct request_buffer {
  struct network_request *requests;
  size_t head;
  size_t size;
  size_t limit;
};

int request_buffer_init(struct request_buffer*, size_t limit);

int request_buffer_destroy(struct request_buffer*);

bool request_buffer_full(struct request_buffer*);

bool request_buffer_empty(struct request_buffer*);

/* caller must check requeue_queue_full first */
struct network_request* request_buffer_push(struct request_buffer*);

struct network_request* request_buffer_head(struct request_buffer*);

void request_buffer_pop(struct request_buffer*);

#endif /* LIBHPX_PARCEL_REQUEST_BUFFER_H_ */
