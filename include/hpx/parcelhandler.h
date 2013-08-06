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
#ifndef LIBHPX_PARCELHANDLER_H_
#define LIBHPX_PARCELHANDLER_H_

#include "hpx/mem.h"
#include "hpx/thread.h"


/* 
---------------------------------------------------------------
 Parcel Handler Queue Data Types
---------------------------------------------------------------
*/

#define CACHE_LINE_SIZE 64 /* TODO: get this for real... */

typedef struct hpx_parcelqueue_node_t {
  void* value; /* BDM: I've kept this void* so we can use it for related things if necessary; we can generalize this queue structure eventually */
  struct hpx_parcelqueue_node_t* next;
} hpx_parcelqueue_node_t;

typedef struct hpx_parcelqueue_t {
  hpx_parcelqueue_node_t* head;
  uint8_t _padding[CACHE_LINE_SIZE - sizeof(hpx_parcelqueue_node_t*)];
  /* padding should improve performance by a fair margin */
  hpx_parcelqueue_node_t* tail;  
  pthread_mutex_t lock;
} hpx_parcelqueue_t;

extern hpx_parcelqueue_t* __hpx_send_queue; /* holds hpx_parcel_serialized_t */

int hpx_parcelqueue_create(hpx_parcelqueue_t**);

void* hpx_parcelqueue_trypop(hpx_parcelqueue_t*);

int hpx_parcelqueue_push(hpx_parcelqueue_t*, void* val);

int hpx_parcelqueue_push_nb(hpx_parcelqueue_t*, void* val);

/* this does NOT deallocate queue */
int hpx_parcelqueue_destroy(hpx_parcelqueue_t**);

/*
 --------------------------------------------------------------------
  Parcel Handler Data

  ctx                          Pointer to the thread context
  thread                       Pointer to the parcel handler's thread
 --------------------------------------------------------------------
*/

typedef struct hpx_parcelhandler_t {
  hpx_context_t *ctx;
  hpx_thread_t *thread;  
} hpx_parcelhandler_t;

/*
 --------------------------------------------------------------------
  Parcel Handler Functions
 --------------------------------------------------------------------
*/

hpx_parcelhandler_t *hpx_parcelhandler_create();
void hpx_parcelhandler_destroy(hpx_parcelhandler_t *);

/*
 --------------------------------------------------------------------
  Private Functions
 --------------------------------------------------------------------
*/

/* TODO: should _hpx_parcelhandler_main be here? */

#endif /* LIBHPX_PARCELHANDLER_H_ */

