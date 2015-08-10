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
#include <libhpx/profiling.h>
#include <libhpx/scheduler.h>
#include <libhpx/system.h>
#include <libhpx/time.h>
#include "network/probe.h"

#ifdef HAVE_APEX
#include "apex.h"
#endif

static hpx_addr_t _hpx_143;
static int _hpx_143_fix_handler(void) {
  _hpx_143 = hpx_gas_alloc_cyclic(sizeof(void*), HPX_LOCALITIES, 0);
  return LIBHPX_OK;
}
static LIBHPX_ACTION(HPX_DEFAULT, 0, _hpx_143_fix, _hpx_143_fix_handler);

/// Cleanup utility function.
///
/// This will delete the global objects, if they've been allocated.
static void _cleanup(locality_t *l) {
  if (!l)
    return;

#ifdef HAVE_APEX 
  // finalize APEX
  apex_finalize();
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
    gas_dealloc(l->gas);
    l->gas = NULL;
  }

  dbg_fini();

  if (l->boot) {
    boot_delete(l->boot);
    l->boot = NULL;
  }

#ifdef HAVE_HWLOC
  if (l->topology) {
    hwloc_topology_destroy(l->topology);
  }
#endif

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

  // Start the internal clock
  libhpx_time_start();

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
#ifdef HAVE_HWLOC
  int e = hwloc_topology_init(&here->topology);
  if (e) {
    status = log_error("failed to initialize a topology.\n");
    goto unwind1;
  }
  e = hwloc_topology_load(here->topology);
  if (e) {
    status = log_error("failed to load the topology.\n");
    goto unwind1;
  }
#endif

  // bootstrap
  here->boot = boot_new(here->config->boot);
  if (!here->boot) {
    status = log_error("failed to bootstrap.\n");
    goto unwind1;
  }
  here->rank = boot_rank(here->boot);
  here->ranks = boot_n_ranks(here->boot);

  // see if we're supposed to output the configuration, only do this at rank 0
  if (config_log_level_isset(here->config, HPX_LOG_CONFIG)) {
    if (here->rank == 0) {
      config_print(here->config, stdout);
    }
  }

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

  prof_init(here->config);

  // Allocate the global heap.
  here->gas = gas_new(here->config, here->boot);
  if (!here->gas) {
    status = log_error("failed to create the global address space.\n");
    goto unwind1;
  }
  HPX_HERE = HPX_THERE(here->rank);

  if (here->config->cores) {
    log_error("--hpx-cores is deprecated, ignoring\n");
  }
  status = system_get_affinity_group_size(pthread_self(), &here->config->cores);
  if (status) {
    goto unwind1;
  }

  if (!here->config->threads) {
    here->config->threads = here->config->cores;
  }
  log("HPX running %d worker threads on %d cores\n", here->config->threads,
      here->config->cores);

  // Initialize the network. This will initialize a transport and, as a side
  // effect initialize our allocators.
  here->network = network_new(here->config, here->boot, here->gas);
  if (!here->network) {
    status = log_error("failed to create network.\n");
    goto unwind1;
  }

  // thread scheduler
  here->sched = scheduler_new(here->config);
  if (!here->sched) {
    status = log_error("failed to create scheduler.\n");
    goto unwind1;
  }

#ifdef HAVE_APEX 
  // initialize APEX, give this main thread a name
  apex_init("HPX WORKER THREAD");
  apex_set_node_id(here->rank);
#endif

  return status;
 unwind1:
  _cleanup(here);
 unwind0:
  return status;
}

/// Called to run HPX.
int _hpx_run(hpx_action_t *act, int n, ...) {
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

  inst_start();

  if (probe_start(here->network) != LIBHPX_OK) {
    status = log_error("could not start network probe\n");
    goto unwind1;
  }

  // create the initial application-level thread to run
  if (here->rank == 0) {
    va_list vargs;
    va_start(vargs, n);
    hpx_parcel_t *p = action_create_parcel_va(HPX_HERE, *act, 0, 0, n, &vargs);
    int status = hpx_parcel_send(p, HPX_NULL);
    va_end(vargs);

    if (status != LIBHPX_OK) {
      log_error("failed to spawn initial action\n");
      goto unwind2;
    }

    // Fix for https://uisapp2.iu.edu/jira-prd/browse/HPX-143
    if (here->ranks > 1 && here->config->gas != HPX_GAS_AGAS) {
      status = hpx_call(HPX_HERE, _hpx_143_fix, HPX_NULL);
      if (status != LIBHPX_OK) {
        log_error("failed to spawn the initial cyclic allocation");
        goto unwind2;
      }
    }
  }

  // start the scheduler, this will return after scheduler_shutdown()
  if (scheduler_startup(here->sched, here->config) != LIBHPX_OK) {
    log_error("scheduler shut down with error.\n");
    goto unwind2;
  }

  // clean up after _hpx_143
  if (_hpx_143 != HPX_NULL) {
    hpx_gas_free(_hpx_143, HPX_NULL);
  }

#if defined(ENABLE_PROFILING) || defined(HAVE_APEX)
  libhpx_stats_print();

  // this will add the stats to the APEX data set
  libhpx_save_apex_stats();
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
    dbg_check(e);
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
