
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Library Unit Test Harness - "Kernel" Threads
  06_kthread.c

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


#include "hpx/hpx.h"


int some_data;

void * __set_some_data(void * ptr) {
  some_data = 73;

  return NULL;
}


/*
 --------------------------------------------------------------------
  TEST: compute core count
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_kthread_get_cores)
{
  long cores;

  cores = hpx_kthread_get_cores();
  ck_assert_msg(cores != 0, "Could not get the number of compute cores on this machine.");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: kernel thread creation & initialization
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_kthread_create)
{
  hpx_kthread_t * kth;
  char msg[128];

  /* create the thread */
  //  some_data = 0;
  //  kth = hpx_kthread_create(__set_some_data, 0, 0);
  //  ck_assert_msg(kth != NULL, "Kernel thread was NULL.");

  //  sleep(1);
  //  hpx_kthread_destroy(kth);
  
  //  sprintf(msg, "Kernel thread did not run the supplied seed function (expected 73, got %d)", some_data);
  //  ck_assert_msg(some_data == 73, msg);
} 
END_TEST

