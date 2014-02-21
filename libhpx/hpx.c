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
#include <string.h>
#include "hpx.h"
#include "builtins.h"
#include "locality.h"
#include "parcel.h"
#include "network.h"
#include "scheduler.h"
#include "thread.h"
#include "future.h"

int
hpx_init(int argc, char * const argv[argc]) {
  // start by initializing all of the subsystems
  int e = HPX_SUCCESS;
  if (unlikely(e = parcel_init_module()))
    goto unwind0;
  if (unlikely(e = thread_init_module()))
    goto unwind1;
  if (unlikely(e = scheduler_init_module()))
    goto unwind2;
  if (unlikely(e = future_init_module()))
    goto unwind3;
  if (unlikely(e == locality_init_module(0)))
    goto unwind4;
  if (unlikely(e = network_init()))
    goto unwind5;

  return e;

 unwind5:
  locality_fini_module();
 unwind4:
  future_fini_module();
 unwind3:
  scheduler_fini_module();
 unwind2:
  thread_fini_module();
 unwind1:
  parcel_fini_module();
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
  if (unlikely(e = thread_init_thread()))
    goto unwind2;
  if (unlikely(e = scheduler_init_thread()))
    goto unwind3;

  return scheduler_startup(act, args, size);

 unwind3:
  thread_fini_thread();
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
hpx_thread_exit(int status, const void *value, unsigned size) {
  // if there's a continuation future, then we set it
  thread_t *thread = thread_current();
  hpx_parcel_t *parcel = thread->parcel;
  hpx_addr_t cont = parcel->cont;
  if (cont != HPX_NULL)
    hpx_future_set(cont, value, size);

  // exit terminates this thread
  scheduler_exit(parcel);
}

void
hpx_call(hpx_addr_t target, hpx_action_t action, void *args, size_t len,
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
