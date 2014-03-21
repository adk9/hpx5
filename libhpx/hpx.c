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

/// ----------------------------------------------------------------------------
/// @file libhpx/hpx.c
/// @brief Implements much of hpx.h using libhpx.
///
/// This file implements the "glue" between the HPX public interface, and
/// libhpx.
/// ----------------------------------------------------------------------------

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "hpx/hpx.h"
#include "libhpx/action.h"
#include "libhpx/debug.h"
#include "libhpx/boot.h"
#include "libhpx/network.h"
#include "libhpx/parcel.h"
#include "libhpx/scheduler.h"
#include "libhpx/system.h"
#include "libhpx/transport.h"

#include "network/allocator.h"
#include "network/heavy.h"


/// ----------------------------------------------------------------------------
/// Global libhpx module objects.
///
/// These are the global objects we allocate and link together in hpx_init().
/// ----------------------------------------------------------------------------
static boot_t           *_boot = NULL;
static transport_t *_transport = NULL;
static allocator_t *_allocator = NULL;
static network_t     *_network = NULL;
static scheduler_t     *_sched = NULL;


/// ----------------------------------------------------------------------------
/// Used to synchronize the main thread during shutdown.
/// ----------------------------------------------------------------------------
static int               _status = HPX_SUCCESS;
static pthread_mutex_t    _mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t _condition = PTHREAD_COND_INITIALIZER;


/// ----------------------------------------------------------------------------
/// Action for use in global free and shutdown.
///
/// TODO: shutdown isn't handled well, in particular, if the scheduler is
///       overloaded then the shutdown action may never happen---ideally the
///       network_progress() loop should deal with shutdown.
/// ----------------------------------------------------------------------------
static int _free_action(void *args);
static int _shutdown_action(void *args);

static hpx_action_t _free = 0;
static hpx_action_t _shutdown = 0;

/// ----------------------------------------------------------------------------
/// The global "here" address.
///
/// This is set with the current rank after _boot is intialized, in hpx_init().
/// ----------------------------------------------------------------------------
hpx_addr_t HPX_HERE = { NULL, -1 };


/// ----------------------------------------------------------------------------
/// The current system state.
/// ----------------------------------------------------------------------------
static enum {
  HPX_NEW = 0,
  HPX_INIT,
  HPX_RUN,
  HPX_SHUTDOWN,
  HPX_ABORT,
  HPX_DONE,
  HPX_INAVLID
} _state = HPX_NEW;

/// ----------------------------------------------------------------------------
/// An action to support hpx_shutdown().
///
/// Acquires the lock, sets the state correctly, and then signals the
/// condition. This allows remote shutdown requests (see hpx_shutdown()).
/// ----------------------------------------------------------------------------
static int _shutdown_action(void *args) {
  int code = *(int*)args;
  pthread_mutex_lock(&_mutex);
  assert(HPX_INIT < _state && _state < HPX_DONE);
  if (HPX_SHUTDOWN > _state) {
    _state = HPX_SHUTDOWN;
    _status = code;
  }
  pthread_cond_signal(&_condition);
  pthread_mutex_unlock(&_mutex);
  hpx_thread_exit(HPX_SUCCESS, NULL, 0);
}


/// Generate an address for a rank.
hpx_addr_t HPX_THERE(int i) {
  hpx_addr_t there = {
    .rank = i,
    .local = NULL
  };
  return there;
}


/// Called by the application to shutdown the scheduler and network. May be
/// called from any lightweight HPX thread, or the network thread.
void hpx_abort(int code) {
  abort();
}


/// Called by the application to terminate the scheduler and network. Called
/// from an HPX lightweight thread.
void hpx_shutdown(int code) {
  for (int i = 0, e = boot_n_ranks(_boot); i < e; ++i) {
    hpx_addr_t l = HPX_THERE(i);
    if (!hpx_addr_eq(l, HPX_HERE))
      hpx_call(l, _shutdown, &code, sizeof(code), HPX_NULL);
  }
  hpx_call(HPX_HERE, _shutdown, &code, sizeof(code), HPX_NULL);
  hpx_thread_exit(HPX_SUCCESS, NULL, 0);
}


/// Allocate and link together all of the library objects.
int hpx_init(const hpx_config_t *cfg) {
  _free = action_register("_hpx_free_action", _free_action);
  _shutdown = action_register("_hpx_shutdown_action", _shutdown_action);
  _boot = boot_new();
  if (!_boot) {
    dbg_error("failed to create boot manager.\n");
    goto unwind0;
  }

  // update the here address
  HPX_HERE.rank = boot_rank(_boot);

  _transport = transport_new(_boot);
  if (!_transport) {
    dbg_error("failed to create transport.\n");
    goto unwind1;
  }

  _allocator = parcel_allocator_new(_transport);
  if (!_allocator) {
    dbg_error("failed to create parcel allocator.\n");
    goto unwind2;
  }

  _network = network_new(_boot, _transport);
  if (!_network) {
    dbg_error("failed to create network.\n");
    goto unwind3;
  }

  int cores = (cfg->cores) ? cfg->cores : system_get_cores();
  int workers = (cfg->threads) ? cfg->threads : cores;
  int stack_size = cfg->stack_bytes;
  _sched = scheduler_new(_network, cores, workers, stack_size, NULL);
  if (!_sched) {
    dbg_error("failed to create scheduler.\n");
    goto unwind4;
  }

  pthread_mutex_lock(&_mutex);
  _state = HPX_INIT;
  pthread_mutex_unlock(&_mutex);

  return HPX_SUCCESS;

 unwind4:
  network_delete(_network);
 unwind3:
  parcel_allocator_delete(_allocator);
 unwind2:
  transport_delete(_transport);
 unwind1:
  boot_delete(_boot);
 unwind0:
  return HPX_ERROR;
}

/// Called to run HPX.
///
/// Currently the main thread sleeps until hpx_shutdown or hpx_abort is called
/// from an HPX thread.
int hpx_run(hpx_action_t act, const void *args, unsigned size) {
  pthread_mutex_lock(&_mutex);

  // Check to see if there was a problem in hpx_init() that the user didn't
  // respond to, of if hpx_init() wasn't called at all. Note we hold _mutex for
  // this check.
  if (_state != HPX_INIT) {
    dbg_error("called with invalid state %d.\n", _state);
    goto exit;
  }

  _state = HPX_RUN;

  // the rank-0 process starts the application by sending a single parcel to
  // itself
  if (boot_rank(_boot) == 0) {
    // allocate and initialize a parcel for the original action
    hpx_parcel_t *p = hpx_parcel_acquire(size);
    if (!p) {
      dbg_error("failed to allocate an initial parcel.\n");
      goto exit;
    }
    p->action = act;
    hpx_parcel_set_data(p, args, size);

    // Don't use hpx_parcel_send() here, because that will try and enqueue the
    // parcel locally, but we're not a scheduler thread. We rely on network
    // loopback for this to work.
    network_send(_network, p);
  }

  // start the network
  pthread_t heavy;
  int e = pthread_create(&heavy, NULL, heavy_network, _network);
  if (e) {
    abort();
  }

  // start the scheduler
  scheduler_startup(_sched);

  // wait for a shutdown or abort to occur
  pthread_cond_wait(&_condition, &_mutex);

  // shut down the system in the correct order
  if (_state == HPX_SHUTDOWN) {
    pthread_cancel(heavy);
    pthread_join(heavy, NULL);
    scheduler_shutdown(_sched);
    scheduler_delete(_sched);
    network_delete(_network);
    parcel_allocator_delete(_allocator);
    transport_delete(_transport);
    boot_delete(_boot);
  }

 exit:
  // return the status that was set out of band
  pthread_mutex_unlock(&_mutex);
  return _status;
}

/// Encapsulates a remote-procedure-call.
int hpx_call(hpx_addr_t target, hpx_action_t action, const void *args,
             size_t len, hpx_addr_t result) {
  hpx_parcel_t *p = hpx_parcel_acquire(len);
  if (!p) {
    dbg_error("could not allocate parcel.\n");
    return 1;
  }

  p->action = action;
  p->target = target;
  p->cont = result;
  hpx_parcel_set_data(p, args, len);
  hpx_parcel_send(p);
  return HPX_SUCCESS;
}


hpx_action_t hpx_register_action(const char *id, hpx_action_handler_t func) {
  return action_register(id, func);
}


void hpx_parcel_send(hpx_parcel_t *p) {
  if (hpx_addr_try_pin(p->target, NULL))
    scheduler_spawn(p);
  else
    network_send(_network, p);
}


/// Allocate a global, block cyclic array.
///
/// The block size is ignored at this point. The entire allocated region is on
/// the calling locality.
hpx_addr_t hpx_global_calloc(size_t n, size_t bytes, size_t block_size,
                             size_t alignment) {
  hpx_addr_t addr = {
    NULL,
    hpx_get_my_rank()
  };

  if (posix_memalign(&addr.local, alignment, n * bytes))
    dbg_error("failed global allocation.\n");
  return addr;
}


int _free_action(void *args) {
  hpx_addr_t addr = hpx_thread_current_target();
  void *local = NULL;
  if (hpx_addr_try_pin(addr, &local))
    free(local);
  hpx_call(addr, _free, NULL, 0, HPX_NULL);
  return HPX_SUCCESS;
}

void hpx_global_free(hpx_addr_t addr) {
  hpx_call(addr, _free, NULL, 0, HPX_NULL);
}


hpx_parcel_t *hpx_parcel_acquire(size_t size) {
  // get a parcel of the right size from the allocator, the returned parcel
  // already has its data pointer and size set appropriately
  hpx_parcel_t *p = parcel_allocator_get(_allocator, size);
  if (!p) {
    dbg_error("failed to get an %lu-byte parcel from the allocator.\n", size);
    return NULL;
  }

  p->pid    = -1;
  p->action = HPX_ACTION_NULL;
  p->target = HPX_HERE;
  p->cont   = HPX_NULL;
  return p;
}


void hpx_parcel_release(hpx_parcel_t *p) {
  parcel_allocator_put(_allocator, p);
}

int hpx_get_my_rank(void) {
  return boot_rank(_boot);
}

int hpx_get_num_ranks(void) {
  return boot_n_ranks(_boot);
}

int hpx_get_num_threads(void) {
  if (!_sched)
    return 0;
  return _sched->n_workers;
}

const char *hpx_get_network_id(void) {
  if (_transport)
    return transport_id(_transport);
  else
    return "cannot query network now";
}
