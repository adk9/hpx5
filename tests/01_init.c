
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Library Unit Test Harness - Initialization and Cleanup Functions
  01_init.c

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


#include "hpx_init.h"


/*
 --------------------------------------------------------------------
  TEST: library initialization
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_init)
{
  hpx_error_t err;

  err = hpx_init();
  ck_assert(err == HPX_SUCCESS);
} 
END_TEST


/*
 --------------------------------------------------------------------
  TEST: library cleanup
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_cleanup)
{
  hpx_cleanup();
}
END_TEST

