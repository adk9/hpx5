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
#include "hpx/network.h"
#include "hpx/parcel.h"
#include "hpx/thread.h"
//#include "hpx/kthread.h"
#include "hpx/mutex.h"

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

/*
--------------------------------------------------------------------
 Parcel Queue 
--------------------------------------------------------------------
*/
/** 
   The handler needs an efficient way to get parcels from other
   threads. This queue is designed to meet that goal. The design goals
   for the queue were:
   
   - it must be safe despite being read to by many threads,
   
   - it must be efficient for the consumer (the parcel handler) since it
   reads from it very often,
   
   - it should be as efficient for the producers as possible given the other constraints.

   It is a locking queue for simplicity. Fortunately, as there is only
   one consumer, we only need a lock on the tail and not one on the
   head. Like in Michael & Scott, we use dummy nodes so that producers
   and the consumer do not block each other. This is especially
   helpful in our case, since pop is supposed to be fast.

   (In fact, in the final implementation, it pretty much is the
   two-lock queue from Michael & Scott with no head lock. See
   http://www.cs.rochester.edu/research/synchronization/pseudocode/queues.html)

   One known issue with the current queue implementation is that one
   thread being killed or dying while pushing to the queue can lock up
   the queue. Is that possible at present? So it may not actually be
   that safe...

*/
typedef struct hpx_parcelqueue_t {
  hpx_parcelqueue_node_t* head;
  uint8_t _padding[CACHE_LINE_SIZE - sizeof(hpx_parcelqueue_node_t*)];
  /* padding should improve performance by a fair margin */
  hpx_parcelqueue_node_t* tail;  
  //  hpx_kthread_mutex_t lock;
  hpx_mutex_t* lock;
} hpx_parcelqueue_t;

extern hpx_parcelqueue_t* __hpx_send_queue; /* holds hpx_parcel_serialized_t */

/** This creates the queue (including any allocation) and initialized its data structures. */
int hpx_parcelqueue_create(hpx_parcelqueue_t**);

/** 
    This pops an element off the queue if one is available, and
    returns NULL otherwise. This function does not block. It should be
    called only by a single consumer.
 */
void* hpx_parcelqueue_trypop(hpx_parcelqueue_t*);

/** This pushes an element onto the queue. It is blocking. It is threadsafe. */
int hpx_parcelqueue_push(hpx_parcelqueue_t*, void* val);

/** 
    This pushed an element onto the queue in a non-blocking, non-threadsafe way. It is to be used when there is only one producer. 
*/
int hpx_parcelqueue_push_nb(hpx_parcelqueue_t*, void* val);

/** This cleans up the queues related data structures and deallocate the queue. */
int hpx_parcelqueue_destroy(hpx_parcelqueue_t**);

/*
 --------------------------------------------------------------------
  Parcel Handler Data

  ctx                          Pointer to the thread context
  thread                       Pointer to the parcel handler's thread
  fut                          Pointer to the parcel handler's return future
 --------------------------------------------------------------------
*/

typedef struct hpx_parcelhandler_t {
  hpx_context_t *ctx;
  hpx_thread_t *thread;  
  hpx_future_t *fut;
} hpx_parcelhandler_t;

/*
 --------------------------------------------------------------------
  Parcel Handler Functions
 --------------------------------------------------------------------
*/

/**
  Create the parcel handler. Returns a newly allocated and initialized
  parcel handler. 

  *Warning*: In the present implementation, there
  should only ever be one parcel handler, or the system will break.
 */
hpx_parcelhandler_t *hpx_parcelhandler_create();

/**
   Clean up the parcel handler: Destroy any outstanding threads in a
   clean way, free any related data structures, and free the parcel
   handler itself.
 */
void hpx_parcelhandler_destroy(hpx_parcelhandler_t *);

/*
 --------------------------------------------------------------------
  Private Functions and Structures
 --------------------------------------------------------------------
*/

/** Contains the arguments needed for the parcel handler to do a get() from a remote node */
struct _hpx_request_list_node_t;
typedef struct _hpx_request_list_node_t _hpx_request_list_node_t;
struct _hpx_request_list_node_t {
  network_request_t request; /* not a pointer so we can avoid extra allocs() */
  hpx_parcel_t* parcel;
  _hpx_request_list_node_t* next;
};

typedef struct _hpx_request_list_t {
  int size;
  _hpx_request_list_node_t* head;
  _hpx_request_list_node_t* tail;
  _hpx_request_list_node_t* prev;
  _hpx_request_list_node_t* curr;
} _hpx_request_list_t;

void _hpx_request_list_init(_hpx_request_list_t* list);

/* Perhaps confusingly, this function returns a network_request_t* so
   that it can be used in the get() call. It's done this way so we can
   avoid an extra alloc() we really don't need. */
network_request_t* _hpx_request_list_append(_hpx_request_list_t* list, hpx_parcel_t* parcel);

/* TODO: Might be nice to have explicitly inline versions of these:
network_request_t* _hpx_request_list_begin(_hpx_request_list_t* list);
network_request_t* _hpx_request_list_curr(_hpx_request_list_t* list);
hpx_parcel_t* _hpx_request_list_curr_parcel(_hpx_request_list_t* list);
void _hpx_request_list_next(_hpx_request_list_t* list);
*/

network_request_t* _hpx_request_list_begin(_hpx_request_list_t* list);

network_request_t* _hpx_request_list_curr(_hpx_request_list_t* list); 

hpx_parcel_t* _hpx_request_list_curr_parcel(_hpx_request_list_t* list);

void _hpx_request_list_next(_hpx_request_list_t* list);

void _hpx_request_list_del(_hpx_request_list_t* list);


/* TODO: should _hpx_parcelhandler_main be here? */

#endif /* LIBHPX_PARCELHANDLER_H_ */

