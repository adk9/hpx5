// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================

#include <unistd.h>
#include "hpx/hpx.h"
#include "tests.h"

static hpx_action_t _typed_task1;
static int _typed_task1_action(int i, float f, char c) {
  printf("Typed task 1 %d %f %c!\n", i, f, c);
  return HPX_SUCCESS;
}
static HPX_ACTION_DEF(TASK, _typed_task1_action, _typed_task1,
                      HPX_INT, HPX_FLOAT, HPX_CHAR);

static hpx_action_t _typed_task2;
static int _typed_task2_action(int i, float f, char c) {
  printf("Typed task 2 %d %f %c!\n", i, f, c);
  sleep(1);
  return HPX_SUCCESS;
}
static HPX_ACTION_DEF(TASK, _typed_task2_action, _typed_task2,
                      HPX_INT, HPX_FLOAT, HPX_CHAR);


static HPX_ACTION(test_libhpx_task, void *UNUSED) {
  int i = 42;
  float f = 1.0;
  char c = 'a';

  printf("Test hpx typed task\n");
  hpx_call_sync(HPX_HERE, _typed_task1, NULL, 0, &i, &f, &c);
  hpx_call_sync(HPX_HERE, _typed_task2, NULL, 0, &i, &f, &c);
  return HPX_SUCCESS;
}


static HPX_ACTION(test_libhpx_task2, void *UNUSED) {
  int i = 42;
  float f = 1.0;
  char c = 'a';

  printf("Test hpx typed task 2\n");
  hpx_addr_t and = hpx_lco_and_new(2);
  hpx_call(HPX_HERE, _typed_task1, and, &i, &f, &c);
  hpx_call(HPX_HERE, _typed_task2, and, &i, &f, &c);
  hpx_lco_wait(and);
  hpx_lco_delete(and, HPX_NULL);
  return HPX_SUCCESS;
} 

TEST_MAIN({
 ADD_TEST(test_libhpx_task);
 ADD_TEST(test_libhpx_task2);
});
