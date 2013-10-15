
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Library Unit Test Harness - Linked Lists
  10_list.c

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

#include "hpx/utils/list.h"
#include "tests.h"


/*
 --------------------------------------------------------------------
  TEST: list size init
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_list_size)
{
  hpx_list_t ll;

  hpx_list_init(&ll);
  ck_assert_msg(hpx_list_size(&ll) == 0, "List size was not 0");
  hpx_list_destroy(&ll);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: insert elements
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_list_insert)
{
  hpx_list_t ll;
  char msg[128];
  int vals[7] = { 104, 42, 73, 91, 14, 8, 57 };
  int x, cnt;

  hpx_list_init(&ll);
  for (x = 0; x < 7; x++) {
    hpx_list_push(&ll, &vals[x]);
  }

  cnt = hpx_list_size(&ll);
  sprintf(msg, "List does not have the correct number of elements (expected 7, got %d).", cnt);
  ck_assert_msg(cnt == 7, msg);

  hpx_list_destroy(&ll);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: take a peek at the head element
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_list_peek)
{
  hpx_list_t ll;
  char msg[128];
  int vals[7] = { 104, 42, 73, 91, 14, 8, 57 };
  int * val;
  int x, cnt;

  hpx_list_init(&ll);
  
  for (x = 0; x < 7; x++) {
    hpx_list_push(&ll, &vals[x]);
  }

  cnt = hpx_list_size(&ll);
  sprintf(msg, "List does not have the correct number of elements (expected 7, got %d)", cnt);
  ck_assert_msg(hpx_list_size(&ll) == 7, msg);

  /* see what the front element is */
  val = hpx_list_peek(&ll);
  sprintf(msg, "List has the wrong front element (expected 57, got %d)", *val);
  ck_assert_msg(*val == 57, msg);

  hpx_list_destroy(&ll);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: pop off some elements
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_list_pop)
{
  hpx_list_t ll;
  char msg[128];
  int vals[12] = { 104, 42, 73, 91, 14, 8, 57, 99, 3, 61, 19, 1087 };
  int * val;
  int x, cnt;

  hpx_list_init(&ll);

  /* insert some values */
  for (x = 0; x < 12; x++) {
    hpx_list_push(&ll, &vals[x]);
  }

  /* delete a few */
  val = hpx_list_pop(&ll);
  sprintf(msg, "List has the wrong front element (expected 1087, got %d)", *val);
  ck_assert_msg(*val == 1087, msg);

  val = hpx_list_pop(&ll);
  sprintf(msg, "List has the wrong front element (expected 19, got %d)", *val);
  ck_assert_msg(*val == 19, msg);

  val = hpx_list_pop(&ll);
  sprintf(msg, "Queue has the wrong front element (expected 61, got %d)", *val);
  ck_assert_msg(*val == 61, msg);

  val = hpx_list_pop(&ll);
  sprintf(msg, "List has the wrong front element (expected 3, got %d)", *val);
  ck_assert_msg(*val == 3, msg);

  /* check the queue size */
  cnt = hpx_list_size(&ll);
  sprintf(msg, "List does not have the correct number of elements (expected 8, got %d)", cnt);
  ck_assert_msg(cnt == 8, msg);

  /* insert and then delete again */
  hpx_list_push(&ll, &vals[11]);
  val = hpx_list_pop(&ll);

  sprintf(msg, "List has the wrong front element (expected 1087, got %d)", *val);
  ck_assert_msg(*val == 1087, msg);

  /* take it down to one element */
  for (x = 0; x < 7; x++) {
    hpx_list_pop(&ll);
  }

  val = hpx_list_peek(&ll);
  sprintf(msg, "List has the wrong front element (expected 104, got %d)", *val);
  ck_assert_msg(*val == 104, msg);

  cnt = hpx_list_size(&ll);
  sprintf(msg, "List does not have the correct number of elements (expected 1, got %d)", cnt);
  ck_assert_msg(cnt == 1, msg);

  /* take it down to no elements */
  hpx_list_pop(&ll);

  cnt = hpx_list_size(&ll);
  sprintf(msg, "List is not empty (got size = %d)", cnt);
  ck_assert_msg(cnt == 0, msg);

  val = hpx_list_peek(&ll);
  ck_assert_msg(val == NULL, "Empty list does not return NULL on peek");

  /* try to pop an empty queue */
  val = hpx_list_pop(&ll);
  ck_assert_msg(val == NULL, "Empty list does not return NULL on pop");

  hpx_list_destroy(&ll);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: iterate through list elements
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_list_iter)
{
  hpx_list_node_t * node = NULL;
  hpx_list_t ll;
  char msg[128];
  int vals[7] = { 104, 42, 73, 91, 14, 8, 57 };
  int * val;
  int x, cnt;

  hpx_list_init(&ll);
  
  for (x = 0; x < 7; x++) {
    hpx_list_push(&ll, &vals[x]);
  }

  cnt = hpx_list_size(&ll);
  sprintf(msg, "List does not have the correct number of elements (expected 7, got %d)", cnt);
  ck_assert_msg(hpx_list_size(&ll) == 7, msg);

  /* get the first node */
  node = hpx_list_first(&ll);
  ck_assert_msg(node != NULL, "First element in the list is NULL.");

  val = node->value;
  sprintf(msg, "First element in the list is incorrect (expected 57, got %d).", *val);
  ck_assert_msg(*val == 57, msg);

  /* get the other nodes */
  for (x = 5; x > 0; x--) {
    node = hpx_list_next(node);
    sprintf(msg, "Element %d in list is NULL.", x);
    ck_assert_msg(node != NULL, msg);

    val = node->value;
    sprintf(msg, "Element %d in list is incorrect (expected %d, got %d).", x, vals[x], *val);
    ck_assert_msg(vals[x] == *val, msg);
  }

  hpx_list_destroy(&ll);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: delete a specific element in the list
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_list_delete)
{
  hpx_list_node_t * node = NULL;
  hpx_list_t ll;
  char msg[128];
  int vals[7] = { 104, 42, 73, 91, 14, 8, 57 };
  int * val;
  int x, cnt;

  hpx_list_init(&ll);
  
  for (x = 0; x < 7; x++) {
    hpx_list_push(&ll, &vals[x]);
  }

  cnt = hpx_list_size(&ll);
  sprintf(msg, "List does not have the correct number of elements (expected 7, got %d)", cnt);
  ck_assert_msg(hpx_list_size(&ll) == 7, msg);

  /* delete an element */
  hpx_list_delete(&ll, &vals[3]);

  cnt = hpx_list_size(&ll);
  sprintf(msg, "List does not have the correct number of elements (expected 6, got %d)", cnt);
  ck_assert_msg(cnt == 6, msg);

  /* see if we have the right values */
  node = hpx_list_first(&ll);
  ck_assert_msg(node != NULL, "First element in the list is NULL");

  val = node->value;
  sprintf(msg, "First element in the list is incorrect (expected 57, got %d)", *val);
  ck_assert_msg(*val == 57, msg);

  node = hpx_list_next(node);
  ck_assert_msg(node != NULL, "Second element in the list is NULL");

  val = node->value;
  sprintf(msg, "Second element in the list is incorrect (expected 8, got %d)", *val);
  ck_assert_msg(*val == 8, msg);

  node = hpx_list_next(node);
  ck_assert_msg(node != NULL, "Third element in the list is NULL");

  val = node->value;
  sprintf(msg, "Third element in the list is incorrect (expected 14, got %d)", *val);
  ck_assert_msg(*val == 14, msg);

  node = hpx_list_next(node);
  ck_assert_msg(node != NULL, "Fourth element in the list is NULL");

  val = node->value;
  sprintf(msg, "Fourth element in the list is incorrect (expected 73, got %d)", *val);
  ck_assert_msg(*val == 73, msg);

  node = hpx_list_next(node);
  ck_assert_msg(node != NULL, "Fifth element in the list is NULL");

  val = node->value;
  sprintf(msg, "Fifth element in the list is incorrect (expected 42, got %d)", *val);
  ck_assert_msg(*val == 42, msg);

  node = hpx_list_next(node);
  ck_assert_msg(node != NULL, "Sixth element in the list is NULL");

  val = node->value;
  sprintf(msg, "Sixth element in the list is incorrect (expected 104, got %d)", *val);
  ck_assert_msg(*val == 104, msg);

  hpx_list_destroy(&ll);
}
END_TEST


/*
  --------------------------------------------------------------------
  register tests from this file
  --------------------------------------------------------------------
*/

void add_10_list(TCase *tc) {
  tcase_add_test(tc, test_libhpx_list_size);
  tcase_add_test(tc, test_libhpx_list_insert);
  tcase_add_test(tc, test_libhpx_list_peek);
  tcase_add_test(tc, test_libhpx_list_pop);
  tcase_add_test(tc, test_libhpx_list_iter);
  tcase_add_test(tc, test_libhpx_list_delete);
}
