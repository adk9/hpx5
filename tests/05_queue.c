
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Library Unit Test Harness - FIFO Queues
  05_queue.c

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

#include <check.h>
#include "hpx/hpx.h"


/*
 --------------------------------------------------------------------
  TEST: queue size init
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_queue_size)
{
  hpx_queue_t q;

  hpx_queue_init(&q);
  ck_assert_msg(hpx_queue_size(&q) == 0, "Queue size was not 0");
  hpx_queue_destroy(&q);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: insert elements
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_queue_insert)
{
  hpx_queue_t q;
  char msg[128];
  int vals[7] = { 104, 42, 73, 91, 14, 8, 57 };
  int x, cnt;

  hpx_queue_init(&q);
  for (x = 0; x < 7; x++) {
    hpx_queue_push(&q, &vals[x]);
  }

  cnt = hpx_queue_size(&q);
  sprintf(msg, "Queue does not have the correct number of elements (expected 7, got %d).", cnt);
  ck_assert_msg(cnt == 7, msg);

  hpx_queue_destroy(&q);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: take a peek at the front element
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_queue_peek)
{
  hpx_queue_t q;
  char msg[128];
  int vals[7] = { 104, 42, 73, 91, 14, 8, 57 };
  int * val;
  int x, cnt;

  hpx_queue_init(&q);
  
  for (x = 0; x < 7; x++) {
    hpx_queue_push(&q, &vals[x]);
  }

  cnt = hpx_queue_size(&q);
  sprintf(msg, "Queue does not have the correct number of elements (expected 7, got %d)", cnt);
  ck_assert_msg(hpx_queue_size(&q) == 7, msg);

  /* see what the front element is */
  val = hpx_queue_peek(&q);
  sprintf(msg, "Queue has the wrong front element (expected 104, got %d)", *val);
  ck_assert_msg(*val == 104, msg);

  hpx_queue_destroy(&q);
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: pop off some elements
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_queue_pop)
{
  hpx_queue_t q;
  char msg[128];
  int vals[12] = { 104, 42, 73, 91, 14, 8, 57, 99, 3, 61, 19, 1087 };
  int * val;
  int x, cnt;

  hpx_queue_init(&q);

  /* insert some values */
  for (x = 0; x < 12; x++) {
    hpx_queue_push(&q, &vals[x]);
  }

  /* delete a few */
  val = hpx_queue_pop(&q);
  sprintf(msg, "Queue has the wrong front element (expected 104, got %d)", *val);
  ck_assert_msg(*val == 104, msg);

  val = hpx_queue_pop(&q);
  sprintf(msg, "Queue has the wrong front element (expected 42, got %d)", *val);
  ck_assert_msg(*val == 42, msg);

  val = hpx_queue_pop(&q);
  sprintf(msg, "Queue has the wrong front element (expected 73, got %d)", *val);
  ck_assert_msg(*val == 73, msg);

  val = hpx_queue_pop(&q);
  sprintf(msg, "Queue has the wrong front element (expected 91, got %d)", *val);
  ck_assert_msg(*val == 91, msg);

  /* check the queue size */
  cnt = hpx_queue_size(&q);
  sprintf(msg, "Queue does not have the correct number of elements (expected 8, got %d)", cnt);
  ck_assert_msg(cnt == 8, msg);

  /* insert and then delete again */
  hpx_queue_push(&q, &vals[11]);
  val = hpx_queue_pop(&q);

  sprintf(msg, "Queue has the wrong front element (expected 14, got %d)", *val);
  ck_assert_msg(*val == 14, msg);

  /* take it down to one element */
  for (x = 0; x < 7; x++) {
    hpx_queue_pop(&q);
  }

  val = hpx_queue_peek(&q);
  sprintf(msg, "Queue has the wrong front element (expected 1087, got %d)", *val);
  ck_assert_msg(*val == 1087, msg);

  cnt = hpx_queue_size(&q);
  sprintf(msg, "Queue does not have the correct number of elements (expected 1, got %d)", cnt);
  ck_assert_msg(cnt == 1, msg);

  /* take it down to no elements */
  hpx_queue_pop(&q);

  cnt = hpx_queue_size(&q);
  sprintf(msg, "Queue is not empty (got size = %d)", cnt);
  ck_assert_msg(cnt == 0, msg);

  val = hpx_queue_peek(&q);
  ck_assert_msg(val == NULL, "Empty queue does not return NULL on peek");

  /* try to pop an empty queue */
  val = hpx_queue_pop(&q);
  ck_assert_msg(val == NULL, "Empty queue does not return NULL on pop");

  hpx_queue_destroy(&q);
}
END_TEST
