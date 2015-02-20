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

/// @file libhpx/scheduler/future.c
/// Defines the future structure.

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libhpx/action.h"
#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/network.h"
#include "libhpx/scheduler.h"
#include "libhpx/transport.h"
#include "libsync/queues.h"
#include "lco.h"
#include "cvar.h"
#include "future.h"

#define dbg_printf0(...)
//#define dbg_printf0 printf
#define dbg_printf(...)
//#define dbg_printf printf
#define FT_SHARED 1<<3

/// This is the guts of the netfuture, containing all locks, condition
/// variables, the state bit array, and the data (or potentially in the
/// future, a pointer to it).
/// This structure is used very differently than the hpx_netfuture_t type.
/// There will be exactly one instance of _netfuture_t for each individual
/// netfuture somewhere in the system. For hpx_netfuture_t on the other hand,
/// there may be as many, and the structure contains no actual data; it is
/// effectively a pointer.
typedef struct {
  lco_t lco;
  cvar_t full;
  cvar_t empty;
  uint32_t bits;
  char data[] HPX_ALIGNED();
} _netfuture_t;

/// This data is used to locate netfutures within the system. An array of
/// these, representing each netfuture array, is stored within the netfuture
/// table at each locality.
typedef struct {
  int table_index;
  hpx_addr_t base_gas;
} _fut_info_t;

/// This struct manages the data the entire netfutures system at this
/// locality needs.
typedef struct {
  lockable_ptr_t lock;
  // must be locked to read or write
  int inited;
  int curr_index;
  int curr_capacity;
  size_t curr_offset; // a new future is allocated at here

  // must be locked for write only
  _fut_info_t *fut_infos;
} _netfuture_table_t;

static hpx_netfuture_config_t _netfuture_cfg = HPX_NETFUTURE_CONFIG_DEFAULTS;

static hpx_action_t _future_set_no_copy_from_remote = 0;
static hpx_action_t _add_future_to_table = 0;
static hpx_action_t _initialize_netfutures = 0;
static hpx_action_t _set_nf_lco = 0;

static _netfuture_table_t _netfuture_table = {.inited = 0};

static int
_set_nf_lco_action(hpx_addr_t lco) {
  hpx_lco_set(lco, 0, NULL, HPX_NULL, HPX_NULL);
  return HPX_SUCCESS;
}

/// Is this netfuture empty?
static bool _empty(const _netfuture_t *f) {
  return f->bits & HPX_UNSET;
}

/// Is this netfuture full?
static bool _full(const _netfuture_t *f) {
  return f->bits & HPX_SET;
}

/// Returns the rank on which a netfuture is located
/// @p f must have the proper netfuture index set.
static int
_netfuture_get_rank(hpx_netfuture_t *f) {
  return f->index % hpx_get_num_ranks();
  // TODO change if we want to allow base_rank != 0
}

/// Returns a count of how many netfutures in a netfuture array are at each
/// locality (which is uniform across all ranks)
static int
_netfutures_at_rank(hpx_netfuture_t *f) {
  return (f->count + hpx_get_num_ranks() - 1)/hpx_get_num_ranks();
}

/// Return the native address of the _netfuture_t representation of a future
/// that MUST BE LOCAL
/// Returns the address of the netfuture itself, not the data it contains.
/// @p f must have the proper netfuture index set.
static uintptr_t
_netfuture_get_addr(hpx_netfuture_t *f) {
  hpx_addr_t base_addr = _netfuture_table.fut_infos[f->table_index].base_gas;
  size_t real_size = sizeof(_netfuture_t) + f->size;
  hpx_addr_t addr_gas;
  addr_gas = hpx_addr_add(base_addr, f->index * real_size, real_size);
  void *addr;
  assert(hpx_gas_try_pin(addr_gas, &addr));
  hpx_gas_unpin(addr_gas);
  return (uintptr_t)addr;
}

/// Return the native address of the _netfuture_t representation of the
/// future's data. The future MUST BE LOCAL
/// Returns the address of the netfuture's data only, not the netfuture
/// itself.
/// @p f must have the proper netfuture index set.
// static uintptr_t
// _netfuture_get_data_addr(hpx_netfuture_t *f) {
//   return _netfuture_get_addr(f) + sizeof(_netfuture_t);
// }

hpx_addr_t _netfuture_get_addr_gas(hpx_netfuture_t *f) {
  hpx_addr_t base = _netfuture_table.fut_infos[f->table_index].base_gas;
  size_t real_size = sizeof(_netfuture_t) + f->size;
  return hpx_addr_add(base, f->index * real_size, real_size);
}

hpx_addr_t _netfuture_get_data_addr_gas(hpx_netfuture_t *f) {
  size_t real_size = sizeof(_netfuture_t) + f->size;
  hpx_addr_t i_base = _netfuture_get_addr_gas(f);
  return hpx_addr_add(i_base, sizeof(_netfuture_t), real_size);
}

/// Will set a netfuture as empty. The caller must hold the lock.
static void
_future_signal_empty(_netfuture_t *f)
{
  f->bits ^= HPX_SET;
  f->bits |= HPX_UNSET;
  cvar_reset(&f->full);
  scheduler_signal_all(&f->empty);
}

/// Will set a netfuture as full. The caller must hold the lock.
static void
_future_signal_full(_netfuture_t *f)
{
  f->bits ^= HPX_UNSET;
  f->bits |= HPX_SET;
  cvar_reset(&f->empty);
  scheduler_signal_all(&f->full);
}

/// This is what we use when we do NOT need to copy memory into the future,
/// as it has been set via RDMA
static void
_future_set_no_copy(_netfuture_t *f) {
  _future_signal_full(f);
}

static int
_future_set_no_copy_from_remote_action(_netfuture_t **fp) {
  _netfuture_t *f = *fp;
  dbg_printf("  _future_set_no_copy_from_remote on netfuture at pa %p\n",
             (void*)f);
  lco_lock(&f->lco);
  if (!_empty(f))
    scheduler_wait(&f->lco.lock, &f->empty);
  _future_set_no_copy(f);
  lco_unlock(&f->lco);
  dbg_printf("  _future_set_no_copy_from_remote on netfuture at pa %p DONE\n",
             (void*)f);
  return HPX_SUCCESS;
}

/// Lock the netfuture table
static void
_table_lock() {
  sync_lockable_ptr_lock(&_netfuture_table.lock);
}

/// Unlock the netfuture table
static void
_table_unlock() {
  sync_lockable_ptr_unlock(&_netfuture_table.lock);
}

typedef struct {
  hpx_netfuture_config_t cfg;
} _nf_init_args_t;

/// Initialize the netfutures system at this locality.
static int
_initialize_netfutures_action(_nf_init_args_t *args) {
  if (hpx_get_my_rank())
    _netfuture_cfg = args->cfg;

  dbg_printf("  Initializing futures on rank %d\n", hpx_get_my_rank());

  _table_lock();
  _netfuture_table.curr_index = 0;
  _netfuture_table.curr_capacity = _netfuture_cfg.max_array_number;
  _netfuture_table.curr_offset = 0;
  _netfuture_table.fut_infos = calloc(_netfuture_table.curr_capacity,
                                      sizeof(_fut_info_t)) ;

  _netfuture_table.inited = 1;

  _table_unlock();

  dbg_printf("  Initialized futures on rank %d\n", hpx_get_my_rank());
  return HPX_SUCCESS;
}

void hpx_netfutures_fini() {
}

hpx_status_t hpx_netfutures_init(hpx_netfuture_config_t *cfg) {
  if (cfg != NULL) {
    assert(cfg->max_size != 0);
    assert(cfg->max_array_number != 0);
    _netfuture_cfg.max_size = cfg->max_size
      + cfg->max_number*sizeof(_netfuture_t);
    _netfuture_cfg.max_number = cfg->max_number;
    _netfuture_cfg.max_array_number = cfg->max_array_number;
  }

  assert(_netfuture_cfg.max_size < UINT32_MAX);

  printf("Initializing netfutures with %zu bytes per rank\n",
         _netfuture_cfg.max_size);

  if (hpx_get_my_rank() != 0)
    return HPX_ERROR;
  hpx_addr_t done = hpx_lco_and_new(hpx_get_num_ranks());
  _nf_init_args_t init_args = {.cfg = _netfuture_cfg};
  for (int i = 0; i < hpx_get_num_ranks(); i++)
    hpx_call(HPX_THERE(i), _initialize_netfutures, done, &init_args,
             sizeof(init_args));
  hpx_lco_wait(done);
  return HPX_SUCCESS;
}

static hpx_status_t
_wait(_netfuture_t *f) {
  if (!_full(f))
    return scheduler_wait(&f->lco.lock, &f->full);
  else
    return cvar_get_error(&f->full);
}

// this version is for when we need to copy memory
static void
_future_set_with_copy(_netfuture_t *f, int size, const void *from)
{
  lco_lock(&f->lco);

  hpx_status_t status;
  if (!_empty(f))
    scheduler_wait(&f->lco.lock, &f->empty);
  else {
    status = cvar_get_error(&f->empty);
    if (status != HPX_SUCCESS)
      goto unlock;
  }

  if (from) {
    //    void *ptr = NULL;
    //    memcpy(&ptr, &f->data, sizeof(ptr));       // strict aliasing
    //    memcpy(ptr, from, size);
    memcpy(f->data, from, size);
  }

  _future_signal_full(f);
 unlock:
  lco_unlock(&f->lco);
}

/// Copies the appropriate value into @p out, waiting if the lco isn't set yet.
/// If @p out is NULL, will not copy, but the future will still be waited on
/// correctly, and if @p set_empty is true, the future will still be reset.
///
/// @param       lco The address of the netfuture
/// @param      size The amount of data to expect
/// @param       out A pointer to the local region to copy the data to. May
///                  be NULL
/// @param set_empty Should the future be marked as empty? If
///                  hpx_lco_netfuture_emptyat() will be required, this
///                  should be false.
static hpx_status_t
_future_get(lco_t *lco, int size, void *out, bool set_empty)
{
  _netfuture_t *f = (_netfuture_t *)lco;
  lco_lock(&f->lco);

  hpx_status_t status = _wait(f);

  if (status != HPX_SUCCESS)
    goto unlock;

  if (out) {
    //    void *ptr = NULL;
    //    memcpy(&ptr, &f->data, sizeof(ptr));       // strict aliasing
    memcpy(out, f->data, size);
  }

  if (set_empty)
    _future_signal_empty(f);

 unlock:
  lco_unlock(&f->lco);
  return status;
}

static void _nf_lco_set(lco_t *lco, int size, const void *from) {
  _netfuture_t* nf = (_netfuture_t*)lco;
  //  hpx_status_t status = _future_set_no_copy_from_remote_action(nf);
  _future_set_no_copy_from_remote_action(&nf);
}

static void _nf_lco_fini(lco_t *lco) {
  return;
}

/// Initialize a single netfuture.
/// Does not apply to a netfuture array, only a single netfuture, possibly
/// within an array.
/// @param      f The netfuture to initialize
/// @param   size The amount of data the netfuture can hold, in bytes. (Not
///               presently used, but needed in order to be able to move to
///               non-inline storage for netfutures, if desired.)
/// @param shared Will this be a shared future?
static void
_future_init(_netfuture_t *f, int size, bool shared)
{
  // the future vtable
  static const lco_class_t vtable = {
    .on_fini = _nf_lco_fini,
    .on_error = NULL,
    .on_set = _nf_lco_set,
    .on_get = NULL,
    .on_getref = NULL,
    .on_release = NULL,
    .on_wait = NULL,
    .on_attach = NULL,
    .on_reset = NULL
  };

  lco_init(&f->lco, &vtable);
  cvar_reset(&f->empty);
  cvar_reset(&f->full);
  f->bits = 0 | HPX_UNSET; // future starts out empty
  if (shared)
    f->bits |= FT_SHARED;
  // we don't allocate - the system does that
}

hpx_netfuture_t
hpx_lco_netfuture_new(size_t size) {
  return hpx_lco_netfuture_new_all(1, size);
  //return _netfuture_new(size, false);
}

struct new_all_args {
  int n;
  int base_rank;
  size_t size;
  hpx_addr_t allg;
};

int _update_table(hpx_netfuture_t *f) {
  _netfuture_table.fut_infos[f->table_index].table_index = f->table_index;
  _netfuture_table.fut_infos[f->table_index].base_gas = f->base_gas;
  _netfuture_table.curr_index++;
  return HPX_SUCCESS;
}

int _add_futures(hpx_netfuture_t *f) {
  hpx_netfuture_t fi = *f;
  fi.index = hpx_get_my_rank();

  for (int i = 0; i < _netfutures_at_rank(f); i ++) {
    _netfuture_t *nf = (_netfuture_t*)_netfuture_get_addr(&fi);
    _future_init(nf, f->size, false);
    dbg_printf("  Initing future on rank %d at pa %p\n", hpx_get_my_rank(),
               (void*)nf);
    dbg_printf0("  Initing future on rank %d at pa %p\n", hpx_get_my_rank(),
                (void*)nf);
    fi.index += hpx_get_num_ranks();
  }
  return HPX_SUCCESS;
}

static int
_add_future_to_table_action(hpx_netfuture_t *f) {
  _table_lock();
  hpx_status_t status;
  if (hpx_get_my_rank() == 0) // don't need to update table on rank 0
    status = HPX_SUCCESS;
  else
    status = _update_table(f);
  _add_futures(f);
  _table_unlock();
  return status;
}

hpx_netfuture_t
hpx_lco_netfuture_new_all(int n, size_t size) {
  assert(hpx_get_my_rank() == 0);
  hpx_netfuture_t f;

  int num_futures_per_rank = (n / hpx_get_num_ranks())
    + ((n % hpx_get_num_ranks()) % 2);
  assert(_netfuture_table.curr_offset
         + num_futures_per_rank * (size + sizeof(_netfuture_t))
         <= _netfuture_cfg.max_size);

  assert(_netfuture_table.curr_index < _netfuture_cfg.max_array_number);

  hpx_addr_t base_gas = hpx_gas_global_alloc(n, size + sizeof(_netfuture_t));

  _table_lock();
  f.base_gas = base_gas;
  f.table_index = _netfuture_table.curr_index;
  f.index = 0;
  f.size = size;
  f.count = n;
  _update_table(&f);
  _table_unlock();
  assert(HPX_SUCCESS == hpx_bcast_sync(_add_future_to_table, &f, sizeof(f)));


  dbg_printf("Done initing futures.");

  return f;
}

// Application level programmer doesn't know how big the future is, so we
// provide this array indexer.
hpx_netfuture_t
hpx_lco_netfuture_at(hpx_netfuture_t array, int i) {
  //  assert(i >= 0 && i <= array.count);
  hpx_netfuture_t fut = array;
  fut.index = i;
  return fut;
}

void hpx_lco_netfuture_setat(hpx_netfuture_t future, int id, size_t size,
                             hpx_addr_t value, hpx_addr_t lsync) {
  assert(id >= 0 && id <= future.count);
  void *data;
  int e = hpx_gas_try_pin(value, &data); // TODO: unpin this
  dbg_check(e, "failed to pin netfuture\n");

  hpx_netfuture_t future_i = hpx_lco_netfuture_at(future, id);

  dbg_printf("  Setating to %d (%d, future at ga %p) from %d\n",
             future_i.index, _netfuture_get_rank(&future_i),
             (void*)_netfuture_get_addr_gas(&future_i), hpx_get_my_rank());

  if (DEBUG) {
    void *lco_addr;
    hpx_gas_try_pin(lsync, &lco_addr);
    dbg_printf0("  Setating to %d (%d, future at ga %p data at ga %p)"
                " with lco pa %p from %d\n", future_i.index,
                _netfuture_get_rank(&future_i),
                (void*)_netfuture_get_addr_gas(&future_i),
                (void*)_netfuture_get_data_addr_gas(&future_i),
                lco_addr, hpx_get_my_rank());
    hpx_gas_unpin(lsync);
  }

  // normally lco_set does all this
  if (_netfuture_get_rank(&future_i) != hpx_get_my_rank()) {
    dbg_printf0("  Enqueuing setat to %d (rank %d, future at ga %p) from %d\n",
                future_i.index, _netfuture_get_rank(&future_i),
                (void*)_netfuture_get_addr_gas(&future_i), hpx_get_my_rank());

    // PWC HERE
    hpx_addr_t remote_lco_addr = _netfuture_get_addr_gas(&future_i);
    hpx_addr_t remote_addr = _netfuture_get_data_addr_gas(&future_i);
    network_pwc(here->network, remote_addr, data, size,
                hpx_lco_set_action, lsync,
                hpx_lco_set_action, remote_lco_addr);
  }
  else {
    _future_set_with_copy((_netfuture_t*)_netfuture_get_addr(&future_i),
                          size, data);
    hpx_lco_set(lsync, 0, NULL, HPX_NULL, HPX_NULL);
  }

  dbg_printf("  Done setting to (rank %d, ga %p) from %d\n",
             _netfuture_get_rank(&future_i),
             (void*)_netfuture_get_addr_gas(&future_i), hpx_get_my_rank());
}

void hpx_lco_netfuture_emptyat(hpx_netfuture_t base, int i,
                               hpx_addr_t rsync_lco) {
  hpx_netfuture_t future_i = hpx_lco_netfuture_at(base, i);

  assert(i >= 0 && i <= base.count);
  assert(_netfuture_get_rank(&future_i) == hpx_get_my_rank());

  _netfuture_t *f = (_netfuture_t*)_netfuture_get_addr(&future_i);

  assert(_full(f));

  lco_lock(&f->lco);

  _future_signal_empty(f);
  lco_unlock(&f->lco);
}

hpx_addr_t hpx_lco_netfuture_getat(hpx_netfuture_t base, int i, size_t size) {
  assert(i >= 0 && i <= base.count);

  hpx_netfuture_t future_i = hpx_lco_netfuture_at(base, i);
  assert(_netfuture_get_rank(&future_i) == hpx_get_my_rank());

  hpx_addr_t retval = HPX_NULL;
  lco_t *lco = (lco_t*)_netfuture_get_addr(&future_i);

  dbg_printf("  Getating %d from (%d, future at ga %"PRIu64") to %d\n",
             future_i.index, _netfuture_get_rank(&future_i),
             _netfuture_get_addr_gas(&future_i), hpx_get_my_rank());

  retval = _netfuture_get_data_addr_gas(&future_i);
  _future_get(lco, size, NULL, false);

  dbg_printf("  Done getting from (%d, ga %p) to %d\n",
             _netfuture_get_rank(&future_i),
             (void*)_netfuture_get_addr_gas(&future_i), hpx_get_my_rank());
  return retval;
}

void hpx_lco_netfuture_free(hpx_netfuture_t future) {
  // TODO
}

void hpx_lco_netfuture_free_all(int num, hpx_netfuture_t base) {
  // Ideally this would not need to know the number of futures. But in order to
  // avoid that we would need to add to futures that are created and deleted
  // with _all() functions a pointer to the base future and a count.

  // TODO
}

int
hpx_lco_netfuture_get_rank(hpx_netfuture_t future) {
  return _netfuture_get_rank(&future);
}

static void HPX_CONSTRUCTOR
_future_initialize_actions(void) {
  HPX_REGISTER_ACTION(_future_set_no_copy_from_remote_action,
                      &_future_set_no_copy_from_remote);
  HPX_REGISTER_ACTION(_add_future_to_table_action, &_add_future_to_table);
  HPX_REGISTER_ACTION(_initialize_netfutures_action, &_initialize_netfutures);
  HPX_REGISTER_ACTION(_set_nf_lco_action, &_set_nf_lco);
}
