
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Library Unit Test Harness - Memory Management
  02_mem.c

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


#include "hpx.h"


/*
 --------------------------------------------------------------------
  TEST: shared memory allocation
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_alloc)
{
  char * some_data = NULL;

  some_data = (char *) hpx_alloc(1024 * sizeof(char));
  ck_assert_msg(some_data != NULL, "hpx_alloc returned NULL");
  
  hpx_free(some_data);
  some_data = NULL;
} 
END_TEST

