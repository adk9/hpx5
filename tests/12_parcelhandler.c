
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
#include "hpx_parcelhandler.h"

/*
 --------------------------------------------------------------------
  TEST: parcel handler creation
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_parcelhandler_create)
{
  hpx_parcelhandler_t * ph = NULL;

  ph = hpx_parcelhandler_create();
  ck_assert_msg(ph != NULL, "Could not create parcelhandler");

  hpx_parcelhandler_destroy(ph);

  ph = NULL;
} 
END_TEST

