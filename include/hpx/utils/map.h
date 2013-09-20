/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Map Function Definitions
  hpx_map.c

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  Authors:
    Patrick K. Bohan <pbohan [at] indiana.edu>
 ====================================================================
*/

#pragma once
#ifndef LIBHPX_MAP_H_
#define LIBHPX_MAP_H_

#include <stdint.h>
#include <stdbool.h>
#include "hpx/list.h"

/* this should probably be a prime number */
#define HPX_MAP_DEFAULT_SIZE  1000003


/*
 --------------------------------------------------------------------
  Map Data Structures
 --------------------------------------------------------------------
*/

struct _hpx_map_t;

typedef uint64_t (*hpx_map_hash_fn_t)(struct _hpx_map_t *, void *);
typedef bool (*hpx_map_cmp_fn_t)(void *, void *);
typedef void (*hpx_map_foreach_fn_t)(void *);

typedef struct _hpx_map_t {
  hpx_map_hash_fn_t hash_fn;
  hpx_map_cmp_fn_t  cmp_fn;
  hpx_list_t       *data;
  uint64_t          count;
  uint64_t          sz;
} hpx_map_t;


/*
 --------------------------------------------------------------------
  Map Functions
 --------------------------------------------------------------------
*/

void hpx_map_init(hpx_map_t *, hpx_map_hash_fn_t, hpx_map_cmp_fn_t, uint64_t);
void hpx_map_destroy(hpx_map_t *);

uint64_t hpx_map_count(hpx_map_t *);
uint64_t hpx_map_size(hpx_map_t *);

void hpx_map_insert(hpx_map_t * map, void *);

#endif /* LIBHPX_MAP_H_ */
