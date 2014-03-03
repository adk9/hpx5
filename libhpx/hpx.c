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
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include "hpx.h"
#include "locality.h"
#include "network.h"
#include "scheduler.h"
#include "debug.h"

hpx_action_t HPX_ACTION_NULL = 0;

static int _null_action(void *args) {
  return HPX_SUCCESS;
}

static void HPX_CONSTRUCTOR _init_hpx(void) {
  HPX_ACTION_NULL = locality_action_register("_null_action", _null_action);
}

// We initialize the three primary modules here.
int
hpx_init(const hpx_config_t *config) {
  // start by initializing all of the subsystems
  int e = locality_startup(config);
  if (e) {
    locality_printe("failed to start locality.\n");
    goto unwind0;
  }

  e = scheduler_startup(config);
  if (e) {
    locality_printe("failed to start the scheduler.\n");
    goto unwind1;
  }

  e = network_startup(config);
  if (e) {
    locality_printe("failed to start the network.\n");
    goto unwind2;
  }

  return e;

 unwind2:
  scheduler_shutdown();
 unwind1:
  locality_shutdown();
 unwind0:
  return e;
}

static int _status = HPX_SUCCESS;
static pthread_mutex_t _mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  _condition = PTHREAD_COND_INITIALIZER;

int
hpx_run(hpx_action_t act, const void *args, unsigned size) {
  pthread_mutex_lock(&_mutex);

  hpx_parcel_t *p = hpx_parcel_acquire(size);
  if (!p) {
    locality_printe("failed to allocate a parcel.\n");
  }
  else {
    hpx_parcel_set_action(p, act);
    if (size)
      hpx_parcel_set_data(p, args, size);
    scheduler_spawn(p);
    pthread_cond_wait(&_condition, &_mutex);
  }

  pthread_mutex_unlock(&_mutex);
  network_shutdown();
  scheduler_shutdown();
  locality_shutdown();
  return _status;
}

/// Called by the application to shutdown the scheduler and network.
void
hpx_shutdown(int code) {
  pthread_mutex_lock(&_mutex);
  _status = code;
  pthread_cond_signal(&_condition);
  pthread_mutex_unlock(&_mutex);
  hpx_thread_exit(HPX_SUCCESS, NULL, 0);
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
int
hpx_call(hpx_addr_t target, hpx_action_t action, const void *args,
         size_t len, hpx_addr_t result) {
  hpx_parcel_t *p = hpx_parcel_acquire(len);
  if (!p) {
    fprintf(stderr, "hpx_call() could not allocate parcel.\n");
    return 1;
  }

  hpx_parcel_set_action(p, action);
  hpx_parcel_set_target(p, target);
  hpx_parcel_set_cont(p, result);
  hpx_parcel_set_data(p, args, len);
  hpx_parcel_send(p);
  return HPX_SUCCESS;
}

hpx_action_t
hpx_register_action(const char *id, hpx_action_handler_t func) {
  return locality_action_register(id, func);
}

