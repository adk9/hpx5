
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  FIFO Queue Function Definitions
  hpx_queue.c

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  Authors:
    Patrick K. Bohan <pbohan [at] indiana.edu>

  https://github.com/brandenburg/binomial-heaps/blob/master/heap.h
 ====================================================================
*/

#pragma once
#ifndef LIBHPX_QUEUE_H_
#define LIBHPX_QUEUE_H_

#include <stdint.h>


/*
 --------------------------------------------------------------------
  Queue Data Structures
 --------------------------------------------------------------------
*/

typedef struct _hpx_queue_node_t {
  struct _hpx_queue_node_t * prev;
  struct _hpx_queue_node_t * next;
  void * value;
} hpx_queue_node_t;
  
typedef struct {
  hpx_queue_node_t * head;
  hpx_queue_node_t * tail;
  uint64_t count;
} hpx_queue_t;


/*
 --------------------------------------------------------------------
  Queue Functions
 --------------------------------------------------------------------
*/

void hpx_queue_init(hpx_queue_t *);
uint64_t hpx_queue_size(hpx_queue_t *);
void * hpx_queue_peek(hpx_queue_t *); 
void hpx_queue_push(hpx_queue_t *, void *);
void * hpx_queue_pop(hpx_queue_t *);

#endif
