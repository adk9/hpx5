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

#include "hpx/hpx.h"
#include "libhpx/action.h"
#include "libhpx/boot.h"
#include "libhpx/config.h"
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

#ifdef ENABLE_TAU
#define TAU_DEFAULT 1
#include <TAU.h>
#endif

/// Cleanup utility function.
///
/// This will delete the global objects, if they've been allocated, and return
/// the passed code.
static int _cleanup(locality_t *l, int code) {
#ifdef ENABLE_TAU
          TAU_START("system_cleanup");
#endif
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

  if (l->config)
    free(l->config);

  if (l)
    free(l);

#ifdef ENABLE_TAU
          TAU_STOP("system_cleanup");
#endif
  return code;
}

int hpx_init(int *argc, char ***argv) {
#ifdef ENABLE_TAU
          TAU_START("hpx_init");
#endif
  here = malloc(sizeof(*here));
  if (!here)
    return dbg_error("init: failed to map the local data segment.\n");

  // 0) parse the provided options into a usable configuration
  hpx_config_t *cfg = hpx_parse_options(argc, argv);
  here->config = cfg;

  dbg_log_level = cfg->loglevel;

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
  if (cfg->waitat == HPX_LOCALITY_ALL || cfg->waitat == here->rank)
    dbg_wait();

  // 6) allocate the transport
  here->transport = transport_new(cfg->transport, cfg->reqlimit);
  if (here->transport == NULL)
    return _cleanup(here, dbg_error("init: failed to create transport.\n"));
  dbg_log("initialized the %s transport.\n", transport_id(here->transport));

  here->gas = gas_new(cfg->heapsize, here->boot, here->transport, cfg->gas);
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
  here->sched = scheduler_new(cores, workers, cfg->stacksize,
                              cfg->backoffmax, cfg->statistics);
  if (here->sched == NULL)
    return _cleanup(here, dbg_error("init: failed to create scheduler.\n"));

#ifdef ENABLE_TAU
          TAU_STOP("hpx_init");
#endif
  return HPX_SUCCESS;
}


/// Called to run HPX.
int hpx_run(hpx_action_t act, const void *args, size_t size) {
#ifdef ENABLE_TAU
          TAU_START("hpx_run");
#endif

  // we start a transport server for the transport, if necessary
  // FIXME: move this functionality into the transport initialization, rather
  //        than branching here
  if (here->transport->type != HPX_TRANSPORT_SMP) {
    hpx_parcel_t *p = hpx_parcel_acquire(NULL, 0);
    if (!p)
      return dbg_error("could not allocate a network server parcel");
    hpx_parcel_set_action(p, light_network);
    hpx_parcel_set_target(p, HPX_HERE);

    YIELD_QUEUE_ENQUEUE(&here->sched->yielded, p);
  }

  if (here->rank == 0) {
    // start the main process. enqueue parcels directly---schedulers
    // don't exist yet
    hpx_parcel_t *p = parcel_create(HPX_HERE, act, args, size, HPX_NULL,
                                    HPX_ACTION_NULL, HPX_NULL, true);
    YIELD_QUEUE_ENQUEUE(&here->sched->yielded, p);
  }

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
  int retval = _cleanup(here, e);
#ifdef ENABLE_TAU
          TAU_STOP("hpx_run");
#endif
  return retval; 
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


void system_shutdown(int code) {
#ifdef ENABLE_TAU
          TAU_START("system_shutdown");
#endif
  if (!here || !here->sched)
    dbg_error("hpx_shutdown called without a scheduler.\n");

  scheduler_shutdown(here->sched);
#ifdef ENABLE_TAU
          TAU_STOP("system_shutdown");
#endif
}


/// Called by the application to terminate the scheduler and network.
void
hpx_shutdown(int code) {
#ifdef ENABLE_TAU
          TAU_START("hpx_shutdown");
#endif
  // do an asynchronous broadcast of shutdown requests
  network_set_shutdown_src(here->network, here->rank);
  hpx_bcast(locality_shutdown, &here->rank, sizeof(here->rank), HPX_NULL);
  hpx_thread_exit(code);
#ifdef ENABLE_TAU
          TAU_STOP("hpx_shutdown");
#endif
}


/// Called by the application to shutdown the scheduler and network. May be
/// called from any lightweight HPX thread, or the network thread.
void
hpx_abort(void) {
#ifdef ENABLE_TAU
          TAU_START("hpx_abort");
#endif

  if (here && here->config && here->config->waitonabort)
    dbg_wait();
  assert(here->boot);
  boot_abort(here->boot);
  abort();
#ifdef ENABLE_TAU
          TAU_STOP("hpx_abort");
#endif
}

