/*
 ====================================================================
  High Performance ParalleX Library (libhpx)

  Thread Function Definitions
  hpx_thread.h

  Copyright (c) 2013, Trustees of Indiana University
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <stdlib.h>
#include <hpx.h>
#include "scheduler.h"
#include "ustack.h"


static int _null_startup_action(void *unused) {
  return HPX_SUCCESS;
}

static hpx_action_t _null_startup = HPX_ACTION_NULL;

int
hpx_init(int argc, char * const argv[argc]) {
  int e = 0;
  if ((e = ustack_init()))
    return e;
  if ((e = scheduler_init()))
    return e;
  _null_startup = hpx_action_register("hpx_null_startup", _null_startup_action);
  if (_null_startup == HPX_ACTION_NULL)
    return 1;
  return HPX_SUCCESS;
}

int
hpx_run(hpx_action_t act, const void *args, unsigned size) {
  int e = 0;
  if ((e = ustack_init_thread()))
    return e;
  if ((e = scheduler_init_thread()))
    return e;
  assert(!args || act);
  return scheduler_startup((act) ? act : _null_startup, args, size);
}

void
hpx_shutdown(int value) {
  exit(value);
}

int
hpx_get_my_rank(void) {
  return 0;
}

int
hpx_get_num_ranks(void) {
  return 0;
}

void
hpx_thread_wait(hpx_addr_t future, void *value) {
}

void
hpx_thread_wait_all(unsigned n, hpx_addr_t futures[], void *values[]) {
}

void
hpx_thread_yield(void) {
}

int
hpx_thread_exit(void *value, unsigned size) {
  return 0;
}

hpx_addr_t
hpx_future_new(int size) {
  return HPX_NULL;
}

void
hpx_future_delete(hpx_addr_t future) {
}

void
hpx_call(int rank, hpx_action_t action, void *args, size_t len,
         hpx_addr_t result) {
}

hpx_time_t
hpx_time_now(void) {
  return 0;
}

uint64_t
hpx_time_to_us(hpx_time_t time) {
  return time;
}

uint64_t
hpx_time_to_ms(hpx_time_t time) {
  return time;
}

hpx_addr_t
hpx_addr_from_rank(int rank) {
  return HPX_NULL;
}

int
hpx_addr_to_rank(hpx_addr_t rank) {
  return -1;
}
