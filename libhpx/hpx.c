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

/// @file libhpx/hpx.c
/// @brief Implements much of hpx.h using libhpx.
///
/// This file implements the "glue" between the HPX public interface, and
/// libhpx.

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>

#include "hpx/hpx.h"
#include "libhpx/action.h"
#include "libhpx/boot.h"
#include "libhpx/gas.h"
#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/network.h"
#include "libhpx/newfuture.h"
#include "libhpx/parcel.h"
#include "libhpx/scheduler.h"
#include "libhpx/system.h"
#include "libhpx/transport.h"

#include "network/servers.h"

/// The default configuration.
static const hpx_config_t _default_cfg = HPX_CONFIG_DEFAULTS;

/// Cleanup utility function.
///
/// This will delete the global objects, if they've been allocated, and return
/// the passed code.
static int _cleanup(locality_t *l, int code) {
  if (l->sched) {
    scheduler_delete(l->sched);
    l->sched = NULL;
  }

  if (l->network) {
    network_delete(l->network);
    l->network = NULL;
  }

  if (l->gas) {
    gas_delete(l->gas);
    l->gas = NULL;
  }

  if (l->transport) {
    transport_delete(l->transport);
    l->transport = NULL;
  }

  if (l->boot) {
    boot_delete(l->boot);
    l->boot = NULL;
  }

  return code;
}


static void *_map_local(uint32_t bytes) {
  int prot = PROT_READ | PROT_WRITE;
  int flags = MAP_ANON | MAP_PRIVATE | MAP_NORESERVE;
  return mmap(NULL, bytes, prot, flags, -1, 0);
}


int hpx_init(const hpx_config_t *cfg) {
  // 0) use a default configuration if one is necessary
  if (!cfg)
    cfg = &_default_cfg;

  dbg_log_level = cfg->log_level;
  // 1) start by initializing the entire local data segment
  here = _map_local(UINT32_MAX);
  if (!here)
    return dbg_error("init: failed to map the local data segment.\n");

  // for debugging
  here->rank = -1;
  here->ranks = -1;

  // 2) bootstrap, to figure out some topology information
  here->boot = boot_new(cfg->boot);
  if (here->boot == NULL)
    return _cleanup(here, dbg_error("init: failed to create the bootstrapper.\n"));

  // 3) grab the rank and ranks, these are used all over the place so we expose
  //    them directly
  here->rank = boot_rank(here->boot);
  here->ranks = boot_n_ranks(here->boot);

  // 3a) wait if the user wants us to
  if (cfg->wait == HPX_WAIT)
    if (cfg->wait_at == HPX_LOCALITY_ALL || cfg->wait_at == here->rank)
      dbg_wait();

  // 6) allocate the transport
  here->transport = transport_new(cfg->transport, cfg->req_limit);
  if (here->transport == NULL)
    return _cleanup(here, dbg_error("init: failed to create transport.\n"));
  dbg_log("initialized the %s transport.\n", transport_id(here->transport));

  here->gas = gas_new(cfg->heap_bytes, here->boot, here->transport, cfg->gas);
  if (here->gas == NULL)
    return _cleanup(here, dbg_error("init: failed to create the global address "
                                    "space.\n"));
  int e = here->gas->join();
  if (e)
    return _cleanup(here, dbg_error("init: failed to join the global address "
                                    "space.\n"));

  // 4) update the HPX_HERE global address, depends on GAS initialization
  HPX_HERE = HPX_THERE(here->rank);

  here->network = network_new();
  if (here->network == NULL)
    return _cleanup(here, dbg_error("init: failed to create network.\n"));
  dbg_log("initialized the network.\n");

  int cores = (cfg->cores) ? cfg->cores : system_get_cores();
  int workers = (cfg->threads) ? cfg->threads : cores;
  here->sched = scheduler_new(cores, workers, cfg->stack_bytes,
                              cfg->backoff_max, cfg->statistics);
  if (here->sched == NULL)
    return _cleanup(here, dbg_error("init: failed to create scheduler.\n"));

  // we start a transport server for the transport, if necessary
  // FIXME: move this functionality into the transport initialization, rather
  //        than branching here
  if (here->transport->type != HPX_TRANSPORT_SMP) {
    hpx_parcel_t *p = hpx_parcel_acquire(NULL, 0);
    if (!p)
      return dbg_error("could not allocate a network server parcel");
    hpx_parcel_set_action(p, light_network);
    hpx_parcel_set_target(p, HPX_HERE);

    // enqueue directly---network exists but schedulers don't yet
    network_rx_enqueue(here->network, p);
  }

  return HPX_SUCCESS;
}


/// Called to start up the HPX runtime.
int system_startup(void) {
  // start the scheduler, this will return after scheduler_shutdown()
  int e = scheduler_startup(here->sched);

  // need to flush the transport
  const uint32_t src = network_get_shutdown_src(here->network);
  if (src == here->rank) {
    transport_progress(here->transport, TRANSPORT_FLUSH);
  }
  else {
    dbg_assert(src != UINT32_MAX);
    transport_progress(here->transport, TRANSPORT_CANCEL);
  }

  // and cleanup the system
  return _cleanup(here, e);
}

/// Called to run HPX.
int
hpx_run(hpx_action_t act, const void *args, size_t size) {
  if (here->rank == 0) {
    // start the main process. enqueue parcels directly---schedulers
    // don't exist yet
    hpx_parcel_t *p = parcel_create(HPX_HERE, act, args, size, HPX_NULL,
                                    HPX_ACTION_NULL, 0, true);
    network_rx_enqueue(here->network, p);
  }

  // start the HPX runtime (scheduler) on all of the localities
  return hpx_start();
}


// This function is used to start the HPX scheduler and runtime on
// all of the available localities.
int hpx_start(void) {
  return system_startup();
}


int
hpx_get_my_rank(void) {
  assert(here);
  return here->rank;
}


int
hpx_get_num_ranks(void) {
  assert(here);
  return here->ranks;
}


int
hpx_get_num_threads(void) {
  if (!here || !here->sched)
    return 0;
  return here->sched->n_workers;
}


const char *
hpx_get_network_id(void) {
  if (!here || !here->transport)
    return "cannot query network now";
  return transport_id(here->transport);
}

void system_shutdown(int code) {
  if (!here || !here->sched)
    dbg_error("hpx_shutdown called without a scheduler.\n");

  scheduler_shutdown(here->sched);
}


/// Called by the application to terminate the scheduler and network.
void
hpx_shutdown(int code) {
  // do an asynchronous broadcast of shutdown requests
  network_set_shutdown_src(here->network, here->rank);
  hpx_bcast(locality_shutdown, &here->rank, sizeof(here->rank), HPX_NULL);
  hpx_thread_exit(code);
}


/// Called by the application to shutdown the scheduler and network. May be
/// called from any lightweight HPX thread, or the network thread.
void
hpx_abort(void) {
  assert(here->boot);
  boot_abort(here->boot);
  abort();
}

