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
#include "platform/platform.h"

int
hpx_init(const hpx_config_t *config) {
  // start by initializing all of the subsystems
  int e = HPX_SUCCESS;

  if ((e = platform_init_module()))
    goto unwind0;
  if ((e = parcel_init_module()))
    goto unwind1;
  if ((e = thread_init_module(config->stack_bytes)))
    goto unwind2;
  if ((e = scheduler_init_module()))
    goto unwind3;
  if ((e = future_init_module()))
    goto unwind4;
  if ((e = locality_init_module(config->scheduler_threads)))
    goto unwind5;
  if ((e = network_init_module()))
    goto unwind6;

  return e;

 unwind6:
  locality_fini_module();
 unwind5:
  future_fini_module();
 unwind4:
  scheduler_fini_module();
 unwind3:
  thread_fini_module();
 unwind2:
  parcel_fini_module();
 unwind1:
  platform_fini_module();
 unwind0:
  return e;
}

int
hpx_run(hpx_action_t act, const void *args, unsigned size) {
  assert(act);
  int e = scheduler_startup(act, args, size);
  network_fini_module();
  locality_fini_module();
  future_fini_module();
  scheduler_fini_module();
  thread_fini_module();
  parcel_fini_module();
  platform_fini_module();
  return e;
}

void
hpx_shutdown(int value) {

  exit(value);
}

void
hpx_thread_exit(int status, const void *value, unsigned size) {
  // if there's a continuation future, then we set it
  thread_t *thread = thread_current();
  hpx_parcel_t *parcel = thread->parcel;
  hpx_addr_t cont = parcel->cont;
  if (!hpx_addr_eq(cont, HPX_NULL))
    hpx_future_set(cont, value, size);

  // exit terminates this thread
  scheduler_exit(parcel);
}

void
hpx_call(hpx_addr_t target, hpx_action_t action, const void *args,
         size_t len, hpx_addr_t result) {
  hpx_parcel_t *p = hpx_parcel_acquire(len);
  hpx_parcel_set_action(p, action);
  hpx_parcel_set_target(p, target);
  hpx_parcel_set_cont(p, result);
  hpx_parcel_set_data(p, args, len);
  hpx_parcel_send(p);
}

