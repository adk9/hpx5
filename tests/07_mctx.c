
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Library Unit Test Harness - Machine Context Switching
  07_mctx.c

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


#include <string.h>
#include "hpx_init.h"
#include "hpx_mctx.h"


int * context_counter;

int register_crusher(int a, int b, char c) {
  FILE * dev_null;
  char msg[35];

  sprintf(msg, "I sure hope somebody reads this: %c", c);

  dev_null = fopen("/dev/null", "w");
  if (dev_null != NULL) {
    fwrite(msg, sizeof(char), 35, dev_null);
    fclose(dev_null);
  }

  return (a + b) + 73;
}



/*
 --------------------------------------------------------------------
  TEST: make a context
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_mctx_getcontext)
{
  hpx_mctx_context_t mctx;
  char msg[128];
 
  context_counter = (int *) malloc(sizeof(int));
  if (context_counter != NULL) {
    *context_counter = 0;
  }

  ck_assert_msg(context_counter != NULL, "Could not allocate a counter to test context switching.");
 
  memset(&mctx, 0, sizeof(hpx_mctx_context_t));
  
#ifdef __x86_64__
  hpx_mctx_getcontext(&mctx, &__mcfg);  
  *context_counter += 1;

  /* do something that (hopefully) changes the value of our registers */
  register_crusher(4,92, 'z');

  if (*context_counter < 100) {
    hpx_mctx_setcontext(&mctx, &__mcfg);
  } 

  ck_assert_msg(*context_counter == 100, "Test counter was not incremented in context switch.");
#endif

  free(context_counter);
  context_counter = NULL;
}
END_TEST


