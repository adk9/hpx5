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
#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/network.h"
#include "libhpx/parcel.h"
#include "libhpx/scheduler.h"
#include "libhpx/system.h"
#include "libhpx/transport.h"

#include "network/servers.h"


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


static hpx_action_t _bcast;


typedef struct {
  hpx_action_t action;
  size_t len;
  char *data[];
} _bcast_args_t;


static int _bcast_action(_bcast_args_t *args) {
  hpx_addr_t and = hpx_lco_and_new(here->ranks);
  for (int i = 0, e = here->ranks; i < e; ++i)
    hpx_call(HPX_THERE(i), args->action, args->data, args->len, and);

  hpx_lco_wait(and);
  hpx_lco_delete(and, HPX_NULL);
  return HPX_SUCCESS;
}


static HPX_CONSTRUCTOR void _init_actions(void) {
  _bcast = HPX_REGISTER_ACTION(_bcast_action);
}


int hpx_init(const hpx_config_t *cfg) {
  // 1) start by initializing the entire local data segment
  here = _map_local(UINT32_MAX);
  if (!here)
    return dbg_error("failed to map the local data segment.\n");

  // for debugging
  here->rank = -1;
  here->ranks = -1;

  // 1a) set the local allocation sbrk
  sync_store(&here->local_sbrk, sizeof(*here), SYNC_RELEASE);


  // 2) bootstrap, to figure out some topology information
  here->boot = boot_new(HPX_BOOT_DEFAULT);
  if (here->boot == NULL)
    return _cleanup(here, dbg_error("failed to create boot manager.\n"));

  // 3) grab the rank and ranks, these are used all over the place so we expose
  //    them directly
  here->rank = boot_rank(here->boot);
  here->ranks = boot_n_ranks(here->boot);

  // 3a) wait if the user wants us to
  if (cfg->wait == HPX_WAIT)
    if (cfg->wait_at == HPX_LOCALITY_ALL || cfg->wait_at == here->rank)
      dbg_wait();

  // 3a) set the global allocation sbrk
  sync_store(&here->global_sbrk, here->ranks, SYNC_RELEASE);

  // 4) update the HPX_HERE global address
  HPX_HERE = HPX_THERE(here->rank);

  // 5) allocate our block translation table
  here->btt = btt_new(cfg->gas);
  if (here->btt == NULL)
    return _cleanup(here, dbg_error("failed to create the block translation table.\n"));

  // 6) allocate the transport
  here->transport = transport_new(cfg->transport);
  if (here->transport == NULL)
    return _cleanup(here, dbg_error("failed to create transport.\n"));
  dbg_log("initialized the %s transport.\n", transport_id(here->transport));

  here->network = network_new();
  if (here->network == NULL)
    return _cleanup(here, dbg_error("failed to create network.\n"));
  dbg_log("initialized the network.\n");

  // 7) insert the base mapping for our local data segment, and pin it so that
  //    it doesn't go anywhere, ever....
  btt_insert(here->btt, HPX_HERE, here);
  void *local;
  bool pinned = hpx_gas_try_pin(HPX_HERE, &local);
  assert(local == here);
  assert(pinned);

  int cores = (cfg->cores) ? cfg->cores : system_get_cores();
  int workers = (cfg->threads) ? cfg->threads : cores;
  here->sched = scheduler_new(cores, workers, cfg->stack_bytes,
                              cfg->backoff_max, cfg->statistics);
  if (here->sched == NULL)
    return _cleanup(here, dbg_error("failed to create scheduler.\n"));

  return HPX_SUCCESS;
}


/// Called to run HPX.
int hpx_run(hpx_action_t act, const void *args, unsigned size) {
  // start the network
  // pthread_t heavy;
  // int e = pthread_create(&heavy, NULL, heavy_network, here->network);
  // if (e)
  //   return _cleanup(here, dbg_error("could not start the network
  //   thread.\n"));

  // the rank-0 process starts the application by sending a single parcel to
  // itself
  if (here->rank == 0) {
    // allocate and initialize a parcel for the original action
    hpx_parcel_t *p = hpx_parcel_acquire(NULL, size);
    if (!p)
      return dbg_error("failed to allocate an initial parcel.\n");

    hpx_parcel_set_action(p, act);
    hpx_parcel_set_target(p, HPX_HERE);
    hpx_parcel_set_data(p, args, size);

    // enqueue directly---network exists but schedulers don't yet
    network_rx_enqueue(here->network, p);
  }

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

  // start the scheduler, this will return after scheduler_shutdown()
  int e = scheduler_startup(here->sched);

  // need to flush the transport
  transport_progress(here->transport, true);

  // wait for the network to shutdown
  // e = pthread_join(heavy, NULL);
  // if (e) {
  //   dbg_error("could not join the heavy network thread.\n");
  //   return e;
  // }

  // and cleanup the system
  return _cleanup(here, e);
}

/// A RPC call with a user-specified continuation action.
int
hpx_call_with_continuation(hpx_addr_t addr, hpx_action_t action,
                           const void *args, size_t len,
                           hpx_addr_t c_target, hpx_action_t c_action)
{
  hpx_parcel_t *p = parcel_create(addr, action, (void*)args, len, c_target,
                                  c_action, true);
  if (!p)
    return dbg_error("hpx_call_with_continuation failed.\n");

  hpx_parcel_send_sync(p);
  return HPX_SUCCESS;
}

/// Encapsulates an asynchronous remote-procedure-call.
int
hpx_call(hpx_addr_t addr, hpx_action_t action, const void *args,
         size_t len, hpx_addr_t result)
{
  return hpx_call_with_continuation(addr, action, args, len, result,
                                    hpx_lco_set_action);
}


int
hpx_call_sync(hpx_addr_t addr, hpx_action_t action,
              const void *args, size_t alen,
              void *out, size_t olen)
{
  hpx_addr_t result = hpx_lco_future_new(olen);
  hpx_call(addr, action, args, alen, result);
  int status = hpx_lco_get(result, olen, out);
  hpx_lco_delete(result, HPX_NULL);
  return status;
}

int
hpx_call_async(hpx_addr_t addr, hpx_action_t action,
               const void *args, size_t len,
               hpx_addr_t args_reuse, hpx_addr_t result)
{
  hpx_parcel_t *p = parcel_create(addr, action, args, len, result,
                                  hpx_lco_set_action, false);
  if (!p)
    return dbg_error("hpx_call_async failed.\n");

  hpx_parcel_send(p, args_reuse);
  return HPX_SUCCESS;
}



/// Encapsulates a RPC called on all available localities.
int hpx_bcast(hpx_action_t action, const void *data, size_t len, hpx_addr_t lco) {
  hpx_parcel_t *p = hpx_parcel_acquire(NULL, len + sizeof(_bcast_args_t));
  hpx_parcel_set_target(p, HPX_HERE);
  hpx_parcel_set_action(p, _bcast);
  hpx_parcel_set_cont_action(p, hpx_lco_set_action);
  hpx_parcel_set_cont_target(p, lco);

  _bcast_args_t *args = (_bcast_args_t *)hpx_parcel_get_data(p);
  args->action = action;
  args->len = len;
  memcpy(&args->data, data, len);
  hpx_parcel_send_sync(p);

  return HPX_SUCCESS;
}


hpx_action_t
hpx_register_action(const char *id, hpx_action_handler_t func) {
  return action_register(id, func);
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
  if (!here || !here->sched) {
    dbg_error("hpx_shutdown called without a scheduler.\n");
    abort();
  }

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
  size_t nr = n + (n % ranks);
  uint32_t base_id;
  hpx_call_sync(HPX_THERE(0), locality_global_sbrk, &nr, sizeof(nr), &base_id, sizeof(base_id));

  uint32_t blocks_per_locality = n / ranks + ((n % ranks) ? 1 : 0);
  uint32_t args[3] = {
    base_id,
    blocks_per_locality,
    bytes
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


/// This is a non-collective, local call to allocate memory in the
/// global address space that can be moved.
hpx_addr_t
hpx_gas_alloc(size_t n, uint32_t bytes) {
  assert(here->btt->type != HPX_GAS_NOGLOBAL);

  // Get a set of @p n contiguous block ids.
  uint32_t base_id;
  hpx_call_sync(HPX_THERE(0), locality_global_sbrk, &n, sizeof(n), &base_id, sizeof(base_id));

  uint32_t args[3] = { base_id, n, bytes };
  hpx_addr_t sync = hpx_lco_future_new(0);
  hpx_call(HPX_HERE, locality_alloc_blocks, &args, sizeof(args), sync);
  hpx_lco_wait(sync);
  hpx_lco_delete(sync, HPX_NULL);

  // Return the base id to the caller.
  return hpx_addr_init(0, base_id, bytes);
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

void hpx_gas_global_free(hpx_addr_t addr) {
  dbg_log("unimplemented");
}

hpx_addr_t
hpx_addr_init(uint64_t offset, uint32_t base, uint32_t bytes) {
  assert(bytes != 0);
  hpx_addr_t addr = HPX_ADDR_INIT(offset, base, bytes);
  return addr;
}
