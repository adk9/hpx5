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
#include "libhpx/btt.h"
#include "libhpx/gas.h"
#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/network.h"
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

  if (l->transport) {
    transport_delete(l->transport);
    l->transport = NULL;
  }

  if (l->btt) {
    btt_delete(l->btt);
    l->btt = NULL;
  }

  if (l->gas) {
    gas_delete(l->gas);
    l->gas = NULL;
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

  // 1) set the local allocation sbrk
  sync_store(&here->local_sbrk, sizeof(*here), SYNC_RELEASE);

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

  // 3b) set the global allocation sbrk
  sync_store(&here->global_sbrk, here->ranks, SYNC_RELEASE);

  // 4) update the HPX_HERE global address
  HPX_HERE = HPX_THERE(here->rank);

  // 5) allocate our block translation table
  here->gas = gas_new(cfg->gas, cfg->heap_bytes);
  gas_join(here->gas);
  here->btt = btt_new(cfg->gas, cfg->heap_bytes);
  if (here->btt == NULL)
    return _cleanup(here, dbg_error("init: failed to create the block-translation-table.\n"));

  // 6) allocate the transport
  here->transport = transport_new(cfg->transport);
  if (here->transport == NULL)
    return _cleanup(here, dbg_error("init: failed to create transport.\n"));
  dbg_log("initialized the %s transport.\n", transport_id(here->transport));

  here->network = network_new();
  if (here->network == NULL)
    return _cleanup(here, dbg_error("init: failed to create network.\n"));
  dbg_log("initialized the network.\n");

  // 7) insert the base mapping for our local data segment, and pin it so that
  //    it doesn't go anywhere, ever....
  btt_insert(here->btt, HPX_HERE, here);
  void *local;
  bool pinned = hpx_gas_try_pin(HPX_HERE, &local);
  assert(local == here);
  assert(pinned);

  // 7b) set the global private allocation sbrk
  uint64_t round = ((uint64_t)(UINT32_MAX/here->ranks) * here->ranks) + here->rank;
  uint32_t offset = (uint32_t)((round >= UINT32_MAX) ? (round - here->ranks) : (round));
  dbg_log_gas("initializing private sbrk to %u.\n", offset);
  assert(offset % here->ranks == here->rank);
  sync_store(&here->pvt_sbrk, offset, SYNC_RELEASE);

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
  transport_progress(here->transport, true);

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
  hpx_bcast(locality_shutdown, NULL, 0, HPX_NULL);
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


/// This is currently trying to provide the layout:
///
/// shared [1] T foo[n]; where sizeof(T) == bytes
hpx_addr_t
hpx_gas_global_alloc(size_t n, uint32_t bytes) {
  assert(here->btt->type != HPX_GAS_NOGLOBAL);

  int ranks = here->ranks;

  // Get a set of @p n contiguous block ids.
  uint32_t base_id;
  hpx_call_sync(HPX_THERE(0), locality_global_sbrk, &n, sizeof(n), &base_id, sizeof(base_id));

  uint32_t blocks_per_locality = n / ranks + ((n % ranks) ? 1 : 0);
  uint32_t args[4] = {
    base_id,
    blocks_per_locality,
    bytes,
    0 // zeroed-memory?
  };

  // The global alloc is currently synchronous, because the btt mappings aren't
  // complete until the allocation is complete.
  hpx_addr_t and = hpx_lco_and_new(ranks);
  for (int i = 0; i < ranks; ++i)
    hpx_call(HPX_THERE(i), locality_alloc_blocks, &args, sizeof(args), and);

  hpx_lco_wait(and);
  hpx_lco_delete(and, HPX_NULL);

  // Return the base id to the caller.
  return hpx_addr_init(0, base_id, bytes);
}


hpx_addr_t
hpx_gas_global_calloc(size_t n, uint32_t bytes) {
  assert(here->btt->type != HPX_GAS_NOGLOBAL);

  int ranks = here->ranks;

  // Get a set of @p n contiguous block ids.
  uint32_t base_id;
  hpx_call_sync(HPX_THERE(0), locality_global_sbrk, &n, sizeof(n), &base_id, sizeof(base_id));

  uint32_t blocks_per_locality = n / ranks + ((n % ranks) ? 1 : 0);
  uint32_t args[4] = {
    base_id,
    blocks_per_locality,
    bytes,
    1 // zeroed-memory?
  };

  // The global alloc is currently synchronous, because the btt mappings aren't
  // complete until the allocation is complete.
  hpx_addr_t and = hpx_lco_and_new(ranks);
  for (int i = 0; i < ranks; ++i)
    hpx_call(HPX_THERE(i), locality_alloc_blocks, &args, sizeof(args), and);

  hpx_lco_wait(and);
  hpx_lco_delete(and, HPX_NULL);

  // Return the base id to the caller.
  return hpx_addr_init(0, base_id, bytes);
}


/// This is a non-collective call to allocate a block of memory in the
/// global address space.
hpx_addr_t
hpx_gas_alloc(uint32_t bytes) {
  assert(here->btt->type != HPX_GAS_NOGLOBAL);

  hpx_addr_t addr;
  int ranks = here->ranks;

  // Get a block id.
  uint32_t block_id = sync_addf(&here->pvt_sbrk, -ranks, SYNC_ACQ_REL);
  uint32_t global = sync_load(&here->global_sbrk, SYNC_ACQUIRE);
  if (block_id <= global) {
    dbg_log_gas("gas: rank %d out of blocks for a private allocation of size %u.\n", here->rank, bytes);

    // forward allocation request to a random locality.
    int r = rand() % ranks;
    hpx_call_sync(HPX_THERE(r), locality_gas_alloc, &bytes, sizeof(bytes), &addr, sizeof(addr));
  } else {
    // Insert the block mapping.
    addr = hpx_addr_init(0, block_id, bytes);

    char *block = malloc(bytes);
    assert(block);
    btt_insert(here->btt, addr, block);
  }

  // Return the base id to the caller.
  return addr;
}


bool
hpx_gas_try_pin(const hpx_addr_t addr, void **local) {
  return btt_try_pin(here->btt, addr, local);
}


void
hpx_gas_unpin(const hpx_addr_t addr) {
  btt_unpin(here->btt, addr);
}


void
hpx_gas_move(hpx_addr_t src, hpx_addr_t dst, hpx_addr_t lco) {
  hpx_gas_t type = btt_type(here->btt);
  if ((type == HPX_GAS_PGAS) || (type == HPX_GAS_PGAS_SWITCH)) {
    if (!hpx_addr_eq(lco, HPX_NULL))
      hpx_lco_set(lco, 0, NULL, HPX_NULL, HPX_NULL);
    return;
  }

  hpx_call(dst, locality_gas_move, &src, sizeof(src), lco);
}


static hpx_action_t _gas_free = 0;


/// Perform a global free operation.
///
/// Right now we're leaking the global address space associated with this
/// allocation. In addition, we're not correctly dealing with freeing a
/// distributed array.
///
/// @param unused -
static int
_gas_free_action(void *unused)
{
  hpx_addr_t addr = hpx_thread_current_target();
  void *local = NULL;
  if (!hpx_gas_try_pin(addr, &local))
    return HPX_RESEND;
  free(local);
  hpx_gas_unpin(addr);
  return HPX_SUCCESS;
}


void
hpx_gas_free(hpx_addr_t addr, hpx_addr_t sync)
{
  // currently I don't care about the performance of this call, so we just use
  // the action interface for freeing.
  hpx_call_async(addr, _gas_free, NULL, 0, HPX_NULL, sync);
}

hpx_addr_t
hpx_addr_init(uint64_t offset, uint32_t base, uint32_t bytes) {
  assert(bytes != 0);
  hpx_addr_t addr = HPX_ADDR_INIT(offset, base, bytes);
  return addr;
}

static HPX_CONSTRUCTOR void _init_actions(void) {
  _gas_free = HPX_REGISTER_ACTION(_gas_free_action);
}
