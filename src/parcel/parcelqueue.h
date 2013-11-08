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
#ifndef LIBHPX_PARCEL_PARCELQUEUE_H_
#define LIBHPX_PARCEL_PARCELQUEUE_H_

/*
--------------------------------------------------------------------
 Parcel Queue 
--------------------------------------------------------------------
*/
/** 
   The handler needs an efficient way to get parcels from other
   threads. This queue is designed to meet that goal. The design
   goals for the queue were:
   
   - it must be safe despite being read to by many threads,
   
   - it must be efficient for the consumer (the parcel handler)
     since it reads from it very often,
   
   - it should be as efficient for the producers as possible given
     the other constraints.

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
#define CACHE_LINE_SIZE 64                      /* TODO: get this for real */

#include <stdbool.h>
#include "hpx/types.h"                          /* uint8 */

struct hpx_mutex;                               /* forward declare */
struct pq_node;                                 /* forward declare */

struct parcelqueue {
  struct pq_node* head;
  uint8 padding0[CACHE_LINE_SIZE - sizeof(struct pq_node*)];
  /* padding should improve performance by a fair margin */
  struct pq_node* tail;  
  uint8 padding1[CACHE_LINE_SIZE - sizeof(struct pq_node*)];
  //  hpx_kthread_mutex_t lock;
  struct hpx_mutex* head_lock;
  uint8 padding2[CACHE_LINE_SIZE - sizeof(struct hpx_mutex*)];
  struct hpx_mutex* tail_lock;
};

extern struct parcelqueue* __hpx_send_queue; /* holds hpx_parcel_serialized_t */

/**
 * This creates the queue (including any allocation) and initialized its data
 * structures.
 */ 
int parcelqueue_create(struct parcelqueue**);
int parcelqueue_destroy(struct parcelqueue**);

/**
 * This destroys the queue (including any allocation) and initialized its data
 * structures.
 */ 

/** 
    This pops an element off the queue if one is available, and
    returns NULL otherwise. This function does not block. It should be
    called only by a single consumer.
 */
void* parcelqueue_trypop(struct parcelqueue*);

/**
 * This pushes an element onto the queue. It is blocking. It is threadsafe.
 */
int parcelqueue_push(struct parcelqueue*, void* val);

bool parcelqueue_empty(struct parcelqueue*);

#endif /* LIBHPX_PARCEL_PARCELQUEUE_H_ */
