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

#include <stdio.h>
#include <hpx/hpx.h>

static HPX_ACTION_DECL(id);

static HPX_INTERRUPT(_my_interrupt, void *args) {
  printf("Hi, I am an interrupt!\n");
  return HPX_SUCCESS;
}

static HPX_TASK(_my_task, void *args) {
  printf("Hi, I am a task!\n");
  hpx_call_cc(HPX_HERE, _my_interrupt, NULL, NULL, NULL, 0);
  return HPX_SUCCESS;
}

static HPX_ACTION(_my_action, void *args) {
  printf("Hi, I am an action!\n");
  hpx_call_cc(HPX_HERE, _my_task, NULL, NULL, NULL, 0);
  return HPX_SUCCESS;
}

int _my_typed_action(int i, float f, char c) {
  printf("Hi, I am a typed action with args: %d %f %c!\n", i, f, c);
  hpx_call_cc(HPX_HERE, _my_action, NULL, NULL, NULL, 0);
  return HPX_SUCCESS;
}

static HPX_ACTION_DEF(DEFAULT, _my_typed_action, id, HPX_INT, HPX_FLOAT, HPX_CHAR)

static HPX_ACTION(_main, void *args) {
  int i = 42;
  float f = 1.0;
  char c = 'a';

  hpx_call_sync(HPX_HERE, id, NULL, 0, &i, &f, &c);
  hpx_shutdown(HPX_SUCCESS);
}

int main(int argc, char *argv[]) {
  hpx_init(&argc, &argv);
  return hpx_run(&_main, NULL, 0);
}
