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
#include "builtins.h"
#include "locality.h"
#include "parcel.h"
#include "network.h"
#include "scheduler.h"
#include "ustack.h"

int
hpx_init(int argc, char * const argv[argc]) {
  // start by initializing all of the subsystems
  int e = HPX_SUCCESS;
  if (unlikely(e == locality_init(0)))
    goto unwind0;
  if (unlikely(e = parcel_init()))
    goto unwind1;
  if (unlikely(e = ustack_init()))
    goto unwind2;
  if (unlikely(e = scheduler_init()))
    goto unwind3;
  if (unlikely(e = network_init()))
    goto unwind4;

  return e;

 unwind4:
  scheduler_fini();
 unwind3:
  ustack_fini();
 unwind2:
  parcel_fini();
 unwind1:
  locality_fini();
 unwind0:
  return e;
}

int
hpx_run(hpx_action_t act, const void *args, unsigned size) {
  assert(act);
  int e = 0;
  if (unlikely(e = network_init_thread()))
    goto unwind0;
  if (unlikely(e = parcel_init_thread()))
    goto unwind1;
  if (unlikely(e = ustack_init_thread()))
    goto unwind2;
  if (unlikely(e = scheduler_init_thread()))
    goto unwind3;

  return scheduler_startup(act, args, size);

 unwind3:
  ustack_fini_thread();
 unwind2:
  parcel_fini_thread();
 unwind1:
  network_fini_thread();
 unwind0:
  return e;
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
