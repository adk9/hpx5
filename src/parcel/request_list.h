/*
  ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Parcel Handler Function Definitions
  libhpx/reuest_list.h

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of 
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
  ====================================================================
*/

#pragma once
#ifndef LIBHPX_PARCEL_REQUEST_LIST_H_
#define LIBHPX_PARCEL_REQUEST_LIST_H_

#include <stddef.h>
#include "hpx/system/attributes.h"              /* HPX_ATTRIBUTES() */

struct header;                                  /* forward declare */
struct network_request;                         /* forward declare */

typedef struct request_list_node request_list_node_t;

typedef struct {
  int size;
  request_list_node_t *head;
  request_list_node_t *tail;
  request_list_node_t *prev;
  request_list_node_t *curr;
} request_list_t;

void request_list_init(request_list_t*);

/* Perhaps confusingly, this function returns a network_request_t* so
   that it can be used in the get() call. It's done this way so we can
   avoid an extra alloc() we really don't need. */
struct network_request* request_list_append(request_list_t*, struct header*)
  HPX_ATTRIBUTE(HPX_VISIBILITY_INTERNAL,
                HPX_NON_NULL(1, 2));

void request_list_begin(request_list_t*)
  HPX_ATTRIBUTE(HPX_VISIBILITY_INTERNAL,
                HPX_NON_NULL(1));

struct network_request* request_list_curr(request_list_t*)
  HPX_ATTRIBUTE(HPX_VISIBILITY_INTERNAL,
                HPX_NON_NULL(1));

struct header* request_list_curr_parcel(request_list_t*)
  HPX_ATTRIBUTE(HPX_VISIBILITY_INTERNAL,
                HPX_NON_NULL(1));

size_t request_list_curr_size(request_list_t*)
  HPX_ATTRIBUTE(HPX_VISIBILITY_INTERNAL);

void request_list_next(request_list_t*)
  HPX_ATTRIBUTE(HPX_VISIBILITY_INTERNAL,
                HPX_NON_NULL(1));

void request_list_del(request_list_t*)
  HPX_ATTRIBUTE(HPX_VISIBILITY_INTERNAL,
                HPX_NON_NULL(1));

#endif /* LIBHPX_PARCEL_REQUEST_LIST_H_ */
