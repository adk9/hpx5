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
#ifndef LIBHPX_PARCEL_REQUEST_LIST_H_
#define LIBHPX_PARCEL_REQUEST_LIST_H_

#include "hpx/network.h"                        /* struct network_request */

struct hpx_parcel;                              /* forward declare */

/* !order matters for this struct!
   We depend on the fact that the request_list_node ISA request. */
struct request_list_node {
  struct network_request    request;            /* must be 1st */
  struct hpx_parcel        *parcel;
  struct request_list_node *next;
};

struct request_list {
  int size;
  struct request_list_node *head;
  struct request_list_node *tail;
  struct request_list_node *prev;
  struct request_list_node *curr;
};

void request_list_init(struct request_list*);

/* Perhaps confusingly, this function returns a network_request_t* so
   that it can be used in the get() call. It's done this way so we can
   avoid an extra alloc() we really don't need. */
struct network_request* request_list_append(struct request_list*, struct hpx_parcel*);
void request_list_begin(struct request_list*);
struct network_request* request_list_curr(struct request_list*);
struct hpx_parcel* request_list_curr_parcel(struct request_list*);
void request_list_next(struct request_list*);
void request_list_del(struct request_list*);

#endif /* LIBHPX_PARCEL_REQUEST_LIST_H_ */
