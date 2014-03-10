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
#include "network/network.h"
#include "scheduler/scheduler.h"
#include "parcel.h"
#include "debug.h"

enum {
  HPX_SHUTDOWN = 0,
  HPX_ABORT
};

hpx_action_t HPX_ACTION_NULL = 0;

static int _shutdown_reason = 0;
static int _status = HPX_SUCCESS;
static pthread_mutex_t _mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  _condition = PTHREAD_COND_INITIALIZER;

/// Global null action doesn't do anything.
static int _null_action(void *args) {
  return HPX_SUCCESS;
}

/// Register the global actions.
static void HPX_CONSTRUCTOR _init_actions(void) {
  HPX_ACTION_NULL = hpx_register_action("_null_action", _null_action);
}


/// We initialize the three primary modules here.
int
hpx_init(const hpx_config_t *config) {
  // start by initializing all of the subsystems
  int e = locality_startup(config);
  if (e) {
    dbg_error("failed to start locality.\n");
    goto unwind0;
  }

  e = scheduler_startup(config);
  if (e) {
    dbg_error("failed to start the scheduler.\n");
    goto unwind1;
  }

  e = network_startup(config);
  if (e) {
    dbg_error("failed to start the network.\n");
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

/// called to run HPX---the main thread sleeps until hpx_shutdown or hpx_abort
/// is called from an HPX thread
int
hpx_run(hpx_action_t act, const void *args, unsigned size) {

  pthread_mutex_lock(&_mutex);
  hpx_parcel_t *p = hpx_parcel_acquire(size);
  if (!p) {
    dbg_error("failed to allocate a parcel.\n");
  }
  else {
    hpx_parcel_set_action(p, act);
    hpx_parcel_set_data(p, args, size);
    hpx_parcel_send(p);
    pthread_cond_wait(&_condition, &_mutex);
  }
  pthread_mutex_unlock(&_mutex);

  // shut these down in the right order
  network_shutdown();

  switch (_shutdown_reason) {
   default:
    dbg_error("shutting down for unknown reason %d.\n", _shutdown_reason);
    break;
   case (HPX_ABORT):
    scheduler_abort();
    break;
   case (HPX_SHUTDOWN):
    scheduler_shutdown();
    break;
  }

  locality_shutdown();

  // return the status that was set out of band
  return _status;
}

/// common code to support hpx_abort and hpx_shutdown
static void HPX_NORETURN _shutdown_scheduler(int code, int reason) {
  pthread_mutex_lock(&_mutex);
  _status = code;
  _shutdown_reason = reason;
  pthread_cond_signal(&_condition);
  pthread_mutex_unlock(&_mutex);
  hpx_thread_exit(HPX_SUCCESS, NULL, 0);
}


/// Called by the application to shutdown the scheduler and network.
void
hpx_abort(int code) {
  _shutdown_scheduler(code, HPX_ABORT);
}


void
hpx_shutdown(int code) {
  _shutdown_scheduler(code, HPX_SHUTDOWN);
}


/// Encapsulates a remote-procedure-call.
int
hpx_call(hpx_addr_t target, hpx_action_t action, const void *args,
         size_t len, hpx_addr_t result) {
  hpx_parcel_t *p = hpx_parcel_acquire(len);
  if (!p) {
    dbg_error("could not allocate parcel.\n");
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


void
hpx_parcel_send(hpx_parcel_t *p) {
  if (hpx_addr_try_pin(p->target, NULL))
    scheduler_spawn(p);
  else
    network_send(p);
}
