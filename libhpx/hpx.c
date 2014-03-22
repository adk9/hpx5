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
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

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
/// Action for use in global free and shutdown.
///
/// TODO: shutdown isn't handled well, in particular, if the scheduler is
///       overloaded then the shutdown action may never happen---ideally the
///       network_progress() loop should deal with shutdown.
/// ----------------------------------------------------------------------------
static int _free_action(void *args);
static hpx_action_t _free = 0;


/// ----------------------------------------------------------------------------
/// The global "here" address.
///
/// This is set with the current rank after _boot is intialized, in hpx_init().
/// ----------------------------------------------------------------------------
hpx_addr_t HPX_HERE = { NULL, -1 };


/// Generate an address for a rank.
hpx_addr_t HPX_THERE(int i) {
  hpx_addr_t there = {
    .rank = i,
    .local = NULL
  };
  return there;
}


/// ----------------------------------------------------------------------------
/// Cleanup utility function.
///
/// This will delete the global objects, if they've been allocated, and return
/// the passed code.
/// ----------------------------------------------------------------------------
static int _cleanup(int code) {
  if (_sched) {
    scheduler_delete(_sched);
    _sched = NULL;
  }

  if (_network) {
    network_delete(_network);
    _network = NULL;
  }

  if (_allocator) {
    parcel_allocator_delete(_allocator);
    _allocator = NULL;
  }

  if (_transport) {
    transport_delete(_transport);
    _transport = NULL;
  }

  if (_boot) {
    boot_delete(_boot);
    _boot = NULL;
  }

  return code;
}


/// Allocate and link together all of the library objects.
int hpx_init(const hpx_config_t *cfg) {
  _free = action_register("_hpx_free_action", _free_action);

  _boot = boot_new();
  if (_boot == NULL)
    return _cleanup(dbg_error("failed to create boot manager.\n"));

  // update the here address
  HPX_HERE.rank = boot_rank(_boot);

  _transport = transport_new(_boot);
  if (_transport == NULL)
    return _cleanup(dbg_error("failed to create transport.\n"));

  _allocator = parcel_allocator_new(_transport);
  if (_allocator == NULL)
    return _cleanup(dbg_error("failed to create parcel allocator.\n"));

  _network = network_new(_boot, _transport);
  if (_network == NULL)
    return _cleanup(dbg_error("failed to create network.\n"));

  int cores = (cfg->cores) ? cfg->cores : system_get_cores();
  int workers = (cfg->threads) ? cfg->threads : cores;
  int stack_size = cfg->stack_bytes;
  _sched = scheduler_new(_network, cores, workers, stack_size);
  if (_sched == NULL)
    return _cleanup(dbg_error("failed to create scheduler.\n"));

  return HPX_SUCCESS;
}

/// Called to run HPX.
int hpx_run(hpx_action_t act, const void *args, unsigned size) {
  // start the network
  pthread_t heavy;
  int e = pthread_create(&heavy, NULL, heavy_network, _network);
  if (e)
    return _cleanup(dbg_error("could not start the network thread.\n"));

  // the rank-0 process starts the application by sending a single parcel to
  // itself
  if (boot_rank(_boot) == 0) {
    // allocate and initialize a parcel for the original action
    hpx_parcel_t *p = hpx_parcel_acquire(size);
    if (!p)
      return dbg_error("failed to allocate an initial parcel.\n");

    p->action = act;
    hpx_parcel_set_data(p, args, size);

    // Don't use hpx_parcel_send() here, because that will try and enqueue the
    // parcel locally, but we're not a scheduler thread yet, and haven't
    // initialized the appropriate structures. Network loopback will get this to
    // us once we start progressing and receive from the network.
    network_send(_network, p);
  }

  // start the scheduler, this will return after scheduler_shutdown()
  e = scheduler_startup(_sched);

  // shutdown the network
  network_shutdown(_network);

  // wait for the network to shutdown
  e = pthread_join(heavy, NULL);
  if (e) {
    dbg_error("could not join the heavy network thread.\n");
    return e;
  }

  // and cleanup the system
  return _cleanup(e);
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

/// Called by the application to terminate the scheduler and network.
void hpx_shutdown(int code) {
  if (_sched) {
    scheduler_shutdown(_sched);
    hpx_thread_exit(HPX_SUCCESS, NULL, 0);
  }
  else {
    dbg_error("hpx_shutdown called without a scheduler.\n");
    abort();
  }
}


/// Called by the application to shutdown the scheduler and network. May be
/// called from any lightweight HPX thread, or the network thread.
void hpx_abort(int code) {
  abort();
}
