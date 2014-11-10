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

/// Cleanup utility function.
///
/// This will delete the global objects, if they've been allocated.
static void _cleanup(locality_t *l) {
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
}


int hpx_init(int *argc, char ***argv) {
  hpx_config_t *cfg = hpx_parse_options(argc, argv);
  dbg_log_level = cfg->loglevel;

  if (cfg->waitat == HPX_LOCALITY_ALL || cfg->waitat == here->rank)
    dbg_wait();

  // locality
  here = malloc(sizeof(*here));
  if (!here)
    return dbg_error("failed to allocate a locality.\n");
  here->config = cfg;

  // bootstrap
  here->boot = boot_new(cfg->boot);
  if (!here->boot) {
    _cleanup(here);
    return dbg_error("failed to bootstrap.\n");
  }
  here->rank = boot_rank(here->boot);
  here->ranks = boot_n_ranks(here->boot);

  // byte transport
  here->transport = transport_new(cfg->transport, cfg->reqlimit);
  if (!here->transport) {
    _cleanup(here);
    return dbg_error("failed to create transport.\n");
  }

  // global address space
  here->gas = gas_new(cfg->heapsize, here->boot, here->transport, cfg->gas);
  if (!here->gas) {
    _cleanup(here);
    return dbg_error("failed to create the global address space.\n");
  }
  if (here->gas->join()) {
    _cleanup(here);
    return dbg_error("failed to join the global address space.\n");
  }
  HPX_HERE = HPX_THERE(here->rank);


  // parcel network
  here->network = network_new();
  if (!here->network) {
    _cleanup(here);
    return dbg_error("failed to create network.\n");
  }

  // thread scheduler
  int cores = (cfg->cores) ? cfg->cores : system_get_cores();
  int workers = (cfg->threads) ? cfg->threads : cores;
  here->sched = scheduler_new(cores, workers, cfg->stacksize,
                              cfg->backoffmax, cfg->statistics);
  if (!here->sched) {
    _cleanup(here);
    return dbg_error("failed to create scheduler.\n");
  }

  return HPX_SUCCESS;
}


/// Called to run HPX.
int hpx_run(hpx_action_t act, const void *args, size_t size) {
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
  _cleanup(here);
  return e;
}


int hpx_get_my_rank(void) {
  assert(here);
  return here->rank;
}


int hpx_get_num_ranks(void) {
  assert(here);
  return here->ranks;
}


int hpx_get_num_threads(void) {
  if (!here || !here->sched)
    return 0;
  return here->sched->n_workers;
}


void system_shutdown(int code) {
  if (!here || !here->sched)
    dbg_error("hpx_shutdown called without a scheduler.\n");

  scheduler_shutdown(here->sched);
}


/// Called by the application to terminate the scheduler and network.
void hpx_shutdown(int code) {
  if (!here->ranks) {
    dbg_error("hpx_shutdown can only be called when the system is running.\n");
  }

  // broadcast shutdown to everyone else
  int e = HPX_SUCCESS;
  hpx_addr_t and = hpx_lco_and_new(here->ranks - 1);
  for (uint32_t i = 0, end = here->ranks; i < end; ++i) {
    if (i != here->rank) {
      e = hpx_call(HPX_THERE(i), locality_shutdown, &code, sizeof(code), and);
      dbg_check(e, "failed to broadcast shutdown.\n");
    }
  }
  // and wait until they've all acknowledged the shutdown
  e = hpx_lco_wait(and);
  dbg_check(e, "error while shutting down.\n");
  hpx_lco_delete(and, HPX_NULL);

  // run shutdown here and exit
  e = hpx_call(HPX_HERE, locality_shutdown, &code, sizeof(code), HPX_NULL);
  hpx_thread_exit(e);
}


/// Called by the application to shutdown the scheduler and network. May be
/// called from any lightweight HPX thread, or the network thread.
void hpx_abort(void) {
  if (here && here->config && here->config->waitonabort)
    dbg_wait();
  assert(here->boot);
  boot_abort(here->boot);
  abort();
}

