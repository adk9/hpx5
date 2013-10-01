
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Library Unit Test Harness - Maps
  11_map.c

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

#include <stdint.h>
#include <check.h>
#include "hpx/utils/map.h"

/*
 --------------------------------------------------------------------
  TEST HELPER: map visitor for deletions
 --------------------------------------------------------------------
*/

void int_visitor2(void * ptr) {
  int * i = (int *) ptr;

  ck_assert_msg(*i != -18495, "Element at index 1 was not deleted in the map.");
  ck_assert_msg(*i != -147, "Element at index 2 was not deleted in the map.");
}


/*
 --------------------------------------------------------------------
  TEST HELPER: map visitor
 --------------------------------------------------------------------
*/

void int_visitor(void * ptr) {
  int * i = (int *) ptr;

  *i = 0;  
}


/*
 --------------------------------------------------------------------
  TEST HELPER: hashing function for ints
 --------------------------------------------------------------------
*/

uint64_t int_hasher(hpx_map_t * map, void * ptr) {
  int * n = (int *) ptr;

  return (*n % hpx_map_size(map));
}


/*
 --------------------------------------------------------------------
  TEST HELPER: comparator function for ints
 --------------------------------------------------------------------
*/

bool int_cmp(void * ptr1, void * ptr2) {
  int * i1 = (int *) ptr1;
  int * i2 = (int *) ptr2;

  return (*i1 == *i2);
}


/*
 --------------------------------------------------------------------
  TEST: map initialization size and count
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_map_sizecount)
{
  hpx_map_t map;
  char msg[128];
  uint64_t sz;

  /* initialize a map with default settings */
  hpx_map_init(&map, hpx_thread_map_hash, hpx_thread_map_cmp, 0);

  sz = hpx_map_size(&map);
  sprintf(msg, "Map was initialized with an incorrect size (expected %d, got %ld).", HPX_MAP_DEFAULT_SIZE, sz);
  ck_assert_msg(sz == HPX_MAP_DEFAULT_SIZE, msg);

  sz = hpx_map_count(&map);
  sprintf(msg, "Map was initialized with an incorrect element count (expected 0, got %ld).", sz);
  ck_assert_msg(sz == 0, msg);

  hpx_map_destroy(&map);

  /* initialize a map with a custom size */
  hpx_map_init(&map, hpx_thread_map_hash, hpx_thread_map_cmp, 11287);
  
  sz = hpx_map_size(&map);
  sprintf(msg, "Map was initialized with an incorrect size (expected 11287, got %ld).", sz);
  ck_assert_msg(sz == 11287, msg);

  sz = hpx_map_count(&map);
  sprintf(msg, "Map was initialized with an incorrect element count (expected 0, got %ld).", sz);
  ck_assert_msg(sz == 0, msg);

  hpx_map_destroy(&map);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: map insertions
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_map_insert)
{
  hpx_map_t map;
  int vals[] = { 47, 10284, -18, 9, 10204945, 93, 73, 100 };
  char msg[128];
  int idx;

  hpx_map_init(&map, int_hasher, int_cmp, 23);

  /* insert our values */
  for (idx = 0; idx < 8; idx++) {
    hpx_map_insert(&map, (void *) &vals[idx]);
  }

  /* check our element count */
  sprintf(msg, "Map has an incorrect element count (expected 8, got %ld).", hpx_map_count(&map));
  ck_assert_msg(hpx_map_count(&map) == 8, msg);

  hpx_map_destroy(&map);  
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: map iterations
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_map_foreach)
{
  hpx_map_t map;
  int vals[] = { 729, -18495, -147, 4756, 3, 73, 90, 9, 1, 182734 };
  char msg[128];
  int idx;
 
  hpx_map_init(&map, int_hasher, int_cmp, 5);

  /* insert our values */
  for (idx = 0; idx < 10; idx++) {
    hpx_map_insert(&map, (void *) &vals[idx]);
  }

  /* apply our visitor to the map */
  hpx_map_foreach(&map, int_visitor);

  /* verify that each element got set to 0 */
  for (idx = 0; idx < 10; idx++) {
    sprintf(msg, "Value at index %d was not set correctly (expected 0, got %d).", idx, vals[idx]);
    ck_assert_msg(vals[idx] == 0, msg);
  }

  hpx_map_destroy(&map);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: map deletions
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_map_delete)
{
  hpx_map_t map;
  int vals[] = { 729, -18495, -147, 4756, 3, 73, 90, 9, 1, 182734 };
  char msg[128];
  int idx;
 
  hpx_map_init(&map, int_hasher, int_cmp, 5);

  /* insert our values */
  for (idx = 0; idx < 10; idx++) {
    hpx_map_insert(&map, (void *) &vals[idx]);
  }

  /* delete a couple of values */
  hpx_map_delete(&map, &vals[1]);
  hpx_map_delete(&map, &vals[2]);

  /* try to find the deleted values in the map */
  hpx_map_foreach(&map, int_visitor2);

  /* verify our element count is correct */
  sprintf(msg, "Map has an incorrect element count (expected 8, got %ld).", hpx_map_count(&map));
  ck_assert_msg(hpx_map_count(&map) == 8, msg);

  hpx_map_destroy(&map);  
}
END_TEST
