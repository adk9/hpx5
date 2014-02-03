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

#ifdef HAVE_CONFIG_H
#include "config.h"                             /* HPX_CACHELINE_SIZE */
#endif
#include <stdbool.h>
#include <stdint.h>

typedef struct parcelqueue parcelqueue_t;

/** 
   The handler needs an efficient way to get parcels from other
   threads. This queue is designed to meet that goal. The design
   goals for the queue were:
   
   - it must be safe despite being read to by many threads,
   
   - it must be efficient for the consumer (the parcel handler)
     since it reads from it very often,
   
   - it should be as efficient for the producers as possible given
     the other constraints.

   It is a locking queue for simplicity. This could be changed to
   something faster, potentially.

   One known issue with the current queue implementation is that one
   thread being killed or dying while pushing to the queue can lock up
   the queue. Is that possible at present? So it may not actually be
   that safe...
*/
struct hpx_mutex;                               /* forward declare */
struct pq_node;                                 /* forward declare */

struct parcelqueue {
  struct pq_node* head;
  uint8_t padding0[HPX_CACHELINE_SIZE - sizeof(struct pq_node*)];
  /* padding should improve performance by a fair margin */
  struct pq_node* tail;  
  uint8_t padding1[HPX_CACHELINE_SIZE - sizeof(struct pq_node*)];
  //  hpx_kthread_mutex_t lock;
  struct hpx_mutex* head_lock;
  uint8_t padding2[HPX_CACHELINE_SIZE - sizeof(struct hpx_mutex*)];
  struct hpx_mutex* tail_lock;
};

extern parcelqueue_t* __hpx_send_queue; /* holds hpx_parcel_serialized_t */

/**
 * This creates the queue (including any allocation) and initialized its data
 * structures.
 */ 
int parcelqueue_create(parcelqueue_t**);
int parcelqueue_destroy(parcelqueue_t**);

/**
 * This destroys the queue (including any allocation) and initialized its data
 * structures.
 */ 

/** 
    This pops an element off the queue if one is available, and
    returns NULL otherwise. 
    It is blocking. It is threadsafe.
 */
void* parcelqueue_trypop(parcelqueue_t*);

/**
 * This pushes an element onto the queue. It is blocking. It is threadsafe.
 */
int parcelqueue_push(parcelqueue_t*, void* val);

/**
 * Indicates whether the parcelqueue is empty. It is blocking. It is threadsafe.
 */
bool parcelqueue_empty(struct parcelqueue*);

#endif /* LIBHPX_PARCEL_PARCELQUEUE_H_ */
