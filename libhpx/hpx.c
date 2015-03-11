// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
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
#include <hpx/hpx.h>
#include <libhpx/action.h>
#include <libhpx/boot.h>
#include <libhpx/config.h>
#include <libhpx/debug.h>
#include <libhpx/gas.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/instrumentation.h>
#include <libhpx/memory.h>
#include <libhpx/network.h>
#include <libhpx/scheduler.h>
#include <libhpx/system.h>
#include "network/probe.h"

static hpx_addr_t _hpx_143 = HPX_NULL;
static HPX_ACTION(_hpx_143_fix, void *UNUSED) {
  _hpx_143 = hpx_gas_global_alloc(sizeof(void*), HPX_LOCALITIES);
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

  dbg_fini();

  if (l->boot) {
    boot_delete(l->boot);
    l->boot = NULL;
  }

  hpx_hwloc_topology_destroy(l->topology);

  if (l->actions) {
    action_table_free(l->actions);
  }

  inst_fini();

  if (l->config) {
    config_delete(l->config);
  }

  free(l);
}

int hpx_init(int *argc, char ***argv) {
  int status = HPX_SUCCESS;

  here = malloc(sizeof(*here));
  if (!here) {
    status = log_error("failed to allocate a locality.\n");
    goto unwind0;
  }

  here->rank = -1;
  here->ranks = 0;
  here->actions = NULL;

  here->config = config_new(argc, argv);
  if (!here->config) {
    status = log_error("failed to create a configuration.\n");
    goto unwind1;
  }

  // check to see if everyone is waiting
  if (config_dbg_waitat_isset(here->config, HPX_LOCALITY_ALL)) {
    dbg_wait();
  }

  // topology
  int e = hpx_hwloc_topology_init(&here->topology);
  if (e) {
    status = log_error("failed to initialize a topology.\n");
    goto unwind1;
  }
  e = hpx_hwloc_topology_load(here->topology);
  if (e) {
    status = log_error("failed to load the topology.\n");
    goto unwind1;
  }

  // bootstrap
  here->boot = boot_new(here->config->boot);
  if (!here->boot) {
    status = log_error("failed to bootstrap.\n");
    goto unwind1;
  }
  here->rank = boot_rank(here->boot);
  here->ranks = boot_n_ranks(here->boot);

  // initialize the debugging system
  // @todo We would like to do this earlier but MPI_init() for the bootstrap
  //       network overwrites our segv handler.
  if (LIBHPX_OK != dbg_init(here->config)) {
    goto unwind1;
  }

  // Now that we know our rank, we can be more specific about waiting.
  if (config_dbg_waitat_isset(here->config, here->rank)) {
    // Don't wait twice.
    if (!config_dbg_waitat_isset(here->config, HPX_LOCALITY_ALL)) {
      dbg_wait();
    }
  }

  // Initialize our instrumentation.
  if (inst_init(here->config)) {
    log("error detected while initializing instrumentation\n");
  }

  // Allocate the global heap.
  here->gas = gas_new(here->config, here->boot);
  if (!here->gas) {
    status = log_error("failed to create the global address space.\n");
    goto unwind1;
  }
  if (here->gas->join()) {
    status = log_error("failed to join the global address space.\n");
    goto unwind1;
  }
  HPX_HERE = HPX_THERE(here->rank);

  if (!here->config->cores) {
    here->config->cores = system_get_cores();
  }

  if (!here->config->threads) {
    here->config->threads = here->config->cores;
  }

  // Initialize the network. This will initialize a transport and, as a side
  here->network = network_new(here->config, here->boot, here->gas);
  if (!here->network) {
    status = log_error("failed to create network.\n");
    goto unwind1;
  }
  if (!local || !registered || !global) {
    status = log_error("expected network to initialize address spaces\n");
    goto unwind1;
  }

  // Join the various address spaces.
  // NB: is there a cleaner way to deal with this?
  local->join(local);
  registered->join(registered);
  global->join(global);

  // thread scheduler
  here->sched = scheduler_new(here->config);
  if (!here->sched) {
    status = log_error("failed to create scheduler.\n");
    goto unwind1;
  }

  return status;
 unwind1:
  _cleanup(here);
 unwind0:
  return status;
}

/// Called to run HPX.
int _hpx_run(hpx_action_t *act, int nargs, ...) {
  int status = HPX_SUCCESS;
  if (!here || !here->sched) {
    status = log_error("hpx_init() must be called before hpx_run()\n");
    goto unwind0;
  }

  here->actions = action_table_finalize();
  if (!here->actions) {
    status = log_error("failed to finalize the action table.\n");
    goto unwind0;
  }

  if (probe_start(here->network) != LIBHPX_OK) {
    status = log_error("could not start network probe\n");
    goto unwind1;
  }

  // create the initial application-level thread to run
  if (here->rank == 0) {
    va_list vargs;
    va_start(vargs, nargs);
    status = libhpx_call_action(here->actions, HPX_HERE, *act, HPX_NULL,
                                HPX_ACTION_NULL, HPX_NULL, HPX_NULL,
                                nargs, &vargs);
    va_end(vargs);
    if (status != LIBHPX_OK) {
      log_error("failed to spawn initial action\n");
      goto unwind2;
    }

    // Fix for https://uisapp2.iu.edu/jira-prd/browse/HPX-143
    status = hpx_call(HPX_HERE, _hpx_143_fix, HPX_NULL, NULL, 0);
    if (status != LIBHPX_OK) {
      log_error("failed to spawn the initial cyclic allocation");
      goto unwind2;
    }
  }

  // start the scheduler, this will return after scheduler_shutdown()
  if (scheduler_startup(here->sched) != LIBHPX_OK) {
    log_error("scheduler shut down with error.\n");
    goto unwind2;
  }

  // clean up after _hpx_143
  if (_hpx_143 != HPX_NULL) {
    hpx_gas_free(_hpx_143, HPX_NULL);
  }

#ifdef ENABLE_PROFILING
  scheduler_dump_stats(here->sched);
#endif

 unwind2:
  probe_stop();
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
  dbg_assert_str(here->ranks,
                 "hpx_shutdown can only be called when the system is running.\n");

  // make sure we flush our local network when we shutdown
  network_flush_on_shutdown(here->network);
  for (int i = 0, e = here->ranks; i < e; ++i) {
    int e = network_command(here->network, HPX_THERE(i), locality_shutdown,
                            (uint64_t)code);
    dbg_assert(e == LIBHPX_OK);
  }
  hpx_thread_exit(HPX_SUCCESS);
}

/// Called by the application to shutdown the scheduler and network. May be
/// called from any lightweight HPX thread, or the network thread.
void hpx_abort(void) {
  inst_fini();

  if (here && here->config && here->config->dbg_waitonabort) {
    dbg_wait();
  }
  if (here && here->boot) {
    assert(here->boot);
    boot_abort(here->boot);
  }
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
