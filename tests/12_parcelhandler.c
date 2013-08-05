
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Library Unit Test Harness - Parcel Handler Creation
  12_parchandler.c

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

#include <check.h>
#include "hpx.h"

/*
 --------------------------------------------------------------------
  TEST: parcel handler queue init
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_parcelqueue_create)
{
  bool success;
  int ret;
  hpx_parcelqueue_t* q;
  ret = hpx_parcelqueue_create(&q);
  ck_assert_msg(ret == 0, "Could not initialize parcelqueue");
  hpx_parcelqueue_destroy(&q);
  
}
END_TEST

/*
 --------------------------------------------------------------------
  TEST: parcel handler queue push and pop
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_parcelqueue_push)
{
  int i;
  bool success;
  int ret;
  hpx_parcelqueue_t* q;

  ret = hpx_parcelqueue_create(&q);

  hpx_parcel_t* vals[7];
  for (i = 0; i < 7; i++) {
    vals[i] = hpx_alloc(sizeof(hpx_parcel_t));
    memset(vals[i], 0, sizeof(hpx_parcel_t));
    ret = hpx_parcelqueue_push(q, vals[i]);
    ck_assert_msg(ret == 0, "Could not push to parcelqueue");
  }
  
  hpx_parcel_t* pop_vals[7];
  for (i = 0; i < 7; i++) {
    pop_vals[i] = (hpx_parcel_t*)hpx_parcelqueue_trypop(q);
    ck_assert_msg(pop_vals[i] != NULL, "Could not pop from parcelqueue");
    ck_assert_msg(pop_vals[i] == vals[i], "Popped bad value from parcelqueue");
    hpx_free(pop_vals[i]);
  }
  
  hpx_parcelqueue_destroy(&q);

}
END_TEST



/*
 --------------------------------------------------------------------
  TEST: parcel handler creation
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_parcelhandler_create)
{
  if (__hpx_parcelhandler == NULL) { /* if we've run init, this won't work, and besides we know parcelhandler_create works anyway because it's called by init() */
    hpx_parcelhandler_t * ph = NULL;
    
    ph = hpx_parcelhandler_create();
    ck_assert_msg(ph != NULL, "Could not create parcelhandler");
    
    hpx_parcelhandler_destroy(ph);

    ph = NULL;
  }
} 
END_TEST

