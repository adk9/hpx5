/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Map Functions
  hpx_map.c

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).

  Authors:
    Patrick K. Bohan <pbohan [at] indiana.edu>
 ====================================================================
*/

#include "hpx/map.h"
#include "hpx/mem.h"

/*
 --------------------------------------------------------------------
  hpx_map_init

  Initialize a map.  This function should be called
  before using any other functions on this queue.
 --------------------------------------------------------------------
*/
void hpx_map_init(hpx_map_t *map, hpx_map_hash_fn_t hash_fn, hpx_map_cmp_fn_t cmp_fn, uint64_t sz) {  
  uint64_t idx;

  /* figure out how big our map should be */
  if (sz == 0) {
    map->sz = HPX_MAP_DEFAULT_SIZE;
  } else {
    map->sz = sz;
  }

  /* try to allocate & initialize stuff */
  map->data = (hpx_list_t *) hpx_alloc(map->sz * sizeof(hpx_list_t));
  if (map->data != NULL) {
    map->hash_fn = hash_fn;
    map->cmp_fn = cmp_fn;
    map->count = 0;

    /* init our buckets */
    for (idx = 0; idx < map->sz; idx++) {
      hpx_list_init(&map->data[idx]);
    }
  }
}


/*
 --------------------------------------------------------------------
  hpx_map_destroy

  Frees any memory allocated by this map.  Should be called after
  all other functions.
 --------------------------------------------------------------------
*/
void hpx_map_destroy(hpx_map_t *map) {
  uint64_t idx;

  if (map->data != NULL) {
    for (idx = 0; idx < map->sz; idx++) {
      hpx_list_destroy(&map->data[idx]);
    }

    hpx_free(map->data);
  }
}


/*
 --------------------------------------------------------------------
  hpx_map_count

  Returns the number of elements currently in the map.
 --------------------------------------------------------------------
*/
uint64_t hpx_map_count(hpx_map_t *map) {
  return map->count;
}


/*
 --------------------------------------------------------------------
  hpx_map_size

  Returns the number of buckets allocated for elements in the map.
 --------------------------------------------------------------------
*/
uint64_t hpx_map_size(hpx_map_t *map) {
  return map->sz;
}


/*
 --------------------------------------------------------------------
  hpx_map_insert

  Inserts an element into the map.
 --------------------------------------------------------------------
*/

void hpx_map_insert(hpx_map_t *map, void *ptr) {
  uint64_t idx = map->hash_fn(map, ptr);

  hpx_list_push(&map->data[idx], ptr);
  map->count += 1;
}


/*
 --------------------------------------------------------------------
  hpx_map_delete

  Deletes an element from the map.
 --------------------------------------------------------------------
*/
void hpx_map_delete(hpx_map_t *map, void *ptr) {
  uint64_t idx;
 
  if (map->count > 0) {
    idx = map->hash_fn(map, ptr);
    hpx_list_delete(&map->data[idx], ptr);
    map->count -= 1;  
  }
}


/*
 --------------------------------------------------------------------
  hpx_map_search

  Search for an element in the map.  Returns NULL if no element
  was found with the supplied key.
 --------------------------------------------------------------------
*/
void *hpx_map_search(hpx_map_t *map, void *ptr) {
  hpx_list_node_t *node = NULL;
  uint64_t idx;
  void *val = NULL;

  if (map->count > 0) {
    idx = map->hash_fn(map, ptr);
    node = hpx_list_first(&map->data[idx]);
    while ((val == NULL) && (node != NULL)) {
      if (map->cmp_fn(ptr, node->value)) {
	val = node->value;
      }

      node = hpx_list_next(node);
    }
  }

  return val;
}


/*
 --------------------------------------------------------------------
  hpx_map_foreach

  Iterates through the map in an unspecified order and calls a
  function on each element.
 --------------------------------------------------------------------
*/
void hpx_map_foreach(hpx_map_t *map, hpx_map_foreach_fn_t foreach_fn) {
  hpx_list_node_t *node = NULL;
  uint64_t idx;

  for (idx = 0; idx < map->sz; idx++) {
    node = hpx_list_first(&map->data[idx]);
    while (node != NULL) {
      foreach_fn(node->value);
      node = hpx_list_next(node);
    }
  }
}
