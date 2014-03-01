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
#include "libhpx/locality.h"
#include "libhpx/network.h"
#include "libhpx/scheduler.h"

hpx_action_t HPX_ACTION_NULL = 0;

static int _null_action(void *args) {
  return HPX_SUCCESS;
}

// We initialize the three primary modules here.
int
hpx_init(const hpx_config_t *config) {
  // start by initializing all of the subsystems
  int e = HPX_SUCCESS;

  if ((e = locality_init_module(config)))
    goto unwind0;
  if ((e = scheduler_init_module(config)))
    goto unwind1;
  if ((e = network_init_module(config)))
    goto unwind2;

  HPX_ACTION_NULL = locality_action_register("_null_action", _null_action);
  return e;

 unwind2:
  scheduler_fini_module();
 unwind1:
  locality_fini_module();
 unwind0:
  return e;
}

/// Starts scheduling on the main thread.
int
hpx_run(hpx_action_t act, const void *args, unsigned size) {
  assert(act);
  int e = scheduler_start(act, args, size);
  network_fini_module();
  scheduler_fini_module();
  locality_fini_module();
  return e;
}

/// Called by the application to shutdown the scheduler and network.
void
hpx_shutdown(int value) {
  abort();
  exit(value);
}

/// Exits an HPX user-level thread, with a status and optionally providing a
void
hpx_thread_exit(int status, const void *value, unsigned size) {
  // if there's a continuation future, then we set it
  hpx_parcel_t *parcel = scheduler_current_parcel();
  hpx_addr_t cont = parcel->cont;
  if (!hpx_addr_eq(cont, HPX_NULL))
    hpx_future_set(cont, value, size);

  // exit terminates this thread
  scheduler_exit(parcel);
}

/// Encapsulates a remote-procedure-call.
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

hpx_action_t
hpx_action_register(const char *id, hpx_action_handler_t func) {
  return locality_action_register(id, func);
}

