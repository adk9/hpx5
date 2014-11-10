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

#include <hpx/hpx.h>

#include "libhpx/action.h"
#include "libhpx/boot.h"
#include "libhpx/config.h"
#include "libhpx/debug.h"
#include "libhpx/gas.h"
#include "libhpx/libhpx.h"
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
  if (!cfg) {
    return dbg_error("failed to create a configuration.\n");
  }
  dbg_log_level = cfg->loglevel;
  if (cfg->waitat == HPX_LOCALITY_ALL) {
    dbg_wait();
  }

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
  if (cfg->waitat == here->rank) {
    dbg_wait();
  }

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
  if (scheduler_startup(here->sched) != LIBHPX_OK) {
    return HPX_ERROR;
  }

  network_shutdown(here->network);
  _cleanup(here);
  return HPX_SUCCESS;
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


/// Called by the application to terminate the scheduler and network.
void hpx_shutdown(int code) {
  if (!here->ranks) {
    dbg_error("hpx_shutdown can only be called when the system is running.\n");
  }

  // make sure we flush our local network when we shutdown
  network_flush_on_shutdown(here->network);
  hpx_bcast(locality_shutdown, NULL, 0, HPX_NULL);
  hpx_thread_exit(0);
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

