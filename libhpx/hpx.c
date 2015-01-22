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
# include "config.h"
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
#include "libhpx/scheduler.h"
#include "libhpx/system.h"
#include "libhpx/transport.h"

HPX_DEFDECL_ACTION(ACTION, hpx_143_fix, void *UNUSED) {
  hpx_gas_global_alloc(sizeof(void*), HPX_LOCALITIES);
  return LIBHPX_OK;
}

/// Cleanup utility function.
///
/// This will delete the global objects, if they've been allocated.
static void _cleanup(locality_t *l) {
  if (!l)
    return;

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

  hwloc_topology_destroy(l->topology);

  if (l->actions) {
    action_table_free(l->actions);
  }

  if (l->config) {
    config_free(l->config);
  }

  free(l);
}


int hpx_init(int *argc, char ***argv) {
  hpx_config_t *cfg = parse_options(argc, argv);
  if (!cfg) {
    return dbg_error("failed to create a configuration.\n");
  }
  dbg_log_level = cfg->loglevel;
  if (cfg->waitat == HPX_LOCALITY_ALL) {
    dbg_wait();
  }

  // locality
  here = malloc(sizeof(*here));
  if (!here) {
    return dbg_error("failed to allocate a locality.\n");
  }
  here->rank = -1;
  here->ranks = 0;
  here->actions = NULL;
  here->config = cfg;

  // topology
  int e = hwloc_topology_init(&here->topology);
  if (e) {
    _cleanup(here);
    return dbg_error("failed to initialize a topology.\n");
  }
  e = hwloc_topology_load(here->topology);
  if (e) {
    _cleanup(here);
    return dbg_error("failed to load the topology.\n");
  }

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

  if (cfg->logat && cfg->logat != (int*)HPX_LOCALITY_ALL) {
    int orig_level = dbg_log_level;
    dbg_log_level = 0;
    for (int i = 0; i < cfg->logat[0]; ++i) {
      if (cfg->logat[i+1] == here->rank) {
        dbg_log_level = orig_level;
      }
    }
  }

  // byte transport
  here->transport = transport_new(cfg->transport, cfg->sendlimit, cfg->recvlimit);
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

  int cores = cfg->cores;
  if (!cores) {
    cores = system_get_cores();
  }

  int workers = cfg->threads;
  if (!workers) {
    if (cores == system_get_cores()) {
      workers = cores - 1;
    }
    else {
      workers = cores;
    }
  }

  // parcel network
  here->network = network_new(LIBHPX_NETWORK_DEFAULT, workers);
  if (!here->network) {
    _cleanup(here);
    return dbg_error("failed to create network.\n");
  }

  // thread scheduler
  here->sched = scheduler_new(cores, workers, cfg->stacksize,
                              cfg->backoffmax, cfg->statistics);
  if (!here->sched) {
    _cleanup(here);
    return dbg_error("failed to create scheduler.\n");
  }

  return HPX_SUCCESS;
}


/// Called to run HPX.
int hpx_run(hpx_action_t *act, const void *args, size_t size) {
  int status = HPX_SUCCESS;
  if (!here || !here->sched) {
    status = dbg_error("hpx_init() must be called before hpx_run()\n");
    goto unwind0;
  }

  here->actions = action_table_finalize();
  if (!here->actions) {
    status = dbg_error("failed to finalize the action table.\n");
    goto unwind0;
  }

  if (network_startup(here->network) != LIBHPX_OK) {
    status = dbg_error("could not start network progress\n");
    goto unwind1;
  }

  // create the initial application-level thread to run
  if (here->rank == 0) {
    status = hpx_call(HPX_HERE, *act, args, size, HPX_NULL);
    if (status != LIBHPX_OK) {
      dbg_error("failed to spawn initial action\n");
      goto unwind2;
    }

    // Fix for https://uisapp2.iu.edu/jira-prd/browse/HPX-143
    status = hpx_call(HPX_HERE, hpx_143_fix, NULL, 0, HPX_NULL);
    if (status != LIBHPX_OK) {
      dbg_error("failed to spawn the initial cyclic allocation");
      goto unwind2;
    }
  }

  // start the scheduler, this will return after scheduler_shutdown()
  if (scheduler_startup(here->sched) != LIBHPX_OK) {
    status = dbg_error("scheduler shut down with error.\n");
    goto unwind2;
  }

#ifdef ENABLE_PROFILING
  scheduler_dump_stats(here->sched);
#endif

 unwind2:
  network_shutdown(here->network);
 unwind1:
  _cleanup(here);
 unwind0:
  return status;
}


int hpx_get_my_rank(void) {
  return (here) ? here->rank : -1;
}


int hpx_get_num_ranks(void) {
  return (here && here->boot) ? here->ranks : -1;
}


int hpx_get_num_threads(void) {
  return (here && here->sched) ? here->sched->n_workers : 0;
}


/// Called by the application to terminate the scheduler and network.
void hpx_shutdown(int code) {
  if (!here->ranks) {
    dbg_error("hpx_shutdown can only be called when the system is running.\n");
  }

  // make sure we flush our local network when we shutdown
  network_flush_on_shutdown(here->network);
  int e = hpx_bcast(locality_shutdown, &code, sizeof(code), HPX_NULL);
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

const char *hpx_strerror(hpx_status_t s) {
  switch (s) {
   case (HPX_ERROR): return "HPX_ERROR";
   case (HPX_SUCCESS): return "HPX_SUCCESS";
   case (HPX_RESEND): return "HPX_RESEND";
   case (HPX_LCO_ERROR): return "HPX_LCO_ERROR";
   case (HPX_LCO_CHAN_EMPTY): return "HPX_LCO_CHAN_EMPTY";
   case (HPX_LCO_TIMEOUT): return "HPX_LCO_TIMEOUT";
   case (HPX_LCO_RESET): return "HPX_LCO_RESET";
   case (HPX_USER): return "HPX_USER";
   default: return "HPX undefined error value";
  }
}
