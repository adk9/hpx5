
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Linked List Function Definitions
  hpx_list.c

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
#ifndef LIBHPX_LIST_H_
#define LIBHPX_LIST_H_

#include <stdint.h>
#include <stdlib.h>
//#include <sys/queue.h>


/*
 --------------------------------------------------------------------
  List Data Structures
 --------------------------------------------------------------------
*/

//typedef struct _hpx_list_node_t {
//  SLIST_ENTRY(_hpx_list_node_t) entries;
//  void * value;
//} hpx_list_node_t;
//
//typedef SLIST_HEAD(_hpx_list_head_t, _hpx_list_node_t) hpx_list_head_t;
//
//typedef struct _hpx_list_t {
//  hpx_list_head_t head;
//  uint64_t count;
//} hpx_list_t;

typedef struct _hpx_list_node_t {
  struct _hpx_list_node_t * next;
  void * value;
} hpx_list_node_t;

typedef struct _hpx_list_t {
  struct _hpx_list_node_t * head;
  uint64_t count;
} hpx_list_t;
		   

/*
 --------------------------------------------------------------------
  List Functions
 --------------------------------------------------------------------
*/

void hpx_list_init(hpx_list_t *);
uint64_t hpx_list_size(hpx_list_t *);
void * hpx_list_peek(hpx_list_t *); 
void hpx_list_push(hpx_list_t *, void *);
void * hpx_list_pop(hpx_list_t *);
void hpx_list_delete(hpx_list_t *, void *);

hpx_list_node_t * hpx_list_first(hpx_list_t *);
hpx_list_node_t * hpx_list_next(hpx_list_node_t *);

#endif
