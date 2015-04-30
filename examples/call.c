// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
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

static int _my_interrupt_handler(void) {
  printf("Hi, I am an interrupt!\n");
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_INTERRUPT, 0, _my_interrupt, _my_interrupt_handler);

static int _my_task_handler(void) {
  printf("Hi, I am a task!\n");
  hpx_call_cc(HPX_HERE, _my_interrupt, NULL, NULL);
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_TASK, 0, _my_task, _my_task_handler);

static int _my_action_handler(void) {
  printf("Hi, I am an action!\n");
  hpx_call_cc(HPX_HERE, _my_task, NULL, NULL);
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, 0, _my_action, _my_action_handler);

static int _my_typed_handler(int i, float f, char c) {
  printf("Hi, I am a typed action with args: %d %f %c!\n", i, f, c);
  hpx_call_cc(HPX_HERE, _my_action, NULL, NULL);
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, 0, _my_typed, _my_typed_handler, HPX_INT, HPX_FLOAT,
                  HPX_CHAR);

static int _main_handler(void) {
  int i = 42;
  float f = 1.0;
  char c = 'a';

  hpx_call_sync(HPX_HERE, _my_typed, NULL, 0, &i, &f, &c);
  hpx_shutdown(HPX_SUCCESS);
}
static HPX_ACTION(HPX_DEFAULT, 0, _main, _main_handler);

int main(int argc, char *argv[]) {
  hpx_init(&argc, &argv);
  return hpx_run(&_main);
}
