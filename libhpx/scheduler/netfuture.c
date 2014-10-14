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

#include <photon.h>

#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/scheduler.h"
#include "libhpx/transport.h"
#include "lco.h"
#include "cvar.h"

#define dbg_printf(...)
//#define dbg_printf printf

//#define YIELD_COUNT 10
static hpx_netfuture_config_t _netfuture_cfg = HPX_NETFUTURE_CONFIG_DEFAULTS;
#define PHOTON_NOWAIT_TAG 0
#define FT_SHARED 1<<3
static const int _NETFUTURE_EXCHG = -37;

static bool shutdown = false;

static uint32_t _outstanding_sends = 0;
static uint32_t _outstanding_send_limit = 0;

typedef struct {
  lco_t lco;
  cvar_t full;
  cvar_t empty;
  uint32_t bits;
  int home_rank;
  void* home_address;
  char data[];
} _netfuture_t;

struct _future_wait_args {
  _netfuture_t *fut;
  hpx_set_t set;
  hpx_time_t time;
};

static hpx_action_t _is_shared = 0;

/// Remote block initialization
static hpx_action_t _future_reset_remote = 0;
static hpx_action_t _future_wait_remote = 0;

static hpx_action_t _future_set_no_copy_from_remote = 0;
static hpx_action_t _progress = 0;
static hpx_action_t _add_future_to_table = 0;
static hpx_action_t _initialize_netfutures = 0;

static bool _empty(const _netfuture_t *f) {
  return f->bits & HPX_UNSET;
}

static bool _full(const _netfuture_t *f) {
  return f->bits & HPX_SET;
}

/// Use the LCO's "user" state to remember if the future is in-place or not.
static uintptr_t
_is_inplace(const _netfuture_t *f) {
  return lco_get_user(&f->lco);
}

static int
_is_shared_action(void* args) {
  _netfuture_t *fut;
  hpx_addr_t target = hpx_thread_current_target();
  if (!hpx_gas_try_pin(target, (void**)&fut))
    return HPX_RESEND;

  bool result = (bool)(fut->bits & FT_SHARED);
  hpx_thread_continue(sizeof(bool), &result);
}

typedef struct {
  int table_index;
  int offset; // from base 
} _fut_info_t;

typedef struct {
  lockable_ptr_t lock;
  // must be locked to read or write
  int inited;
  int curr_index;
  int curr_capacity;
  size_t curr_offset; // a new future is allocated at here
  
  // must be locked for write only
  struct photon_buffer_t *buffers; // one for each rank
  void* base;
  hpx_addr_t base_gas;
  size_t mem_size;
  _fut_info_t *fut_infos;
  
} _netfuture_table_t;

static _netfuture_table_t _netfuture_table = {.inited = 0};

lockable_ptr_t pwc_lock;

// which rank is the future on?
static int 
_netfuture_get_rank(hpx_netfuture_t *f) {
  return f->index % hpx_get_num_ranks(); // TODO change if we want to allow base_rank != 0
}

static int
_netfutures_at_rank(hpx_netfuture_t *f) {
  return (f->count + hpx_get_num_ranks() - 1)/hpx_get_num_ranks();
}

static size_t
_netfuture_get_offset(hpx_netfuture_t *f) {
  size_t size = sizeof(_netfuture_t) + f->size;
  return _netfuture_table.fut_infos[f->table_index].offset + (size * (f->index / hpx_get_num_ranks()));
}

// the native address of the _netfuture_t representation of a future
static uintptr_t 
_netfuture_get_addr(hpx_netfuture_t *f) {
  uintptr_t offset =  _netfuture_get_offset(f);
  int rank = _netfuture_get_rank(f);
  uintptr_t rank_base = _netfuture_table.buffers[rank].addr;
  return rank_base + offset;
}

// the native address of the _netfuture_t representation of the future's data
static uintptr_t 
_netfuture_get_data_addr(hpx_netfuture_t *f) {
  return _netfuture_get_addr(f) + sizeof(_netfuture_t);
}

// this is what we use when we do NOT need to copy memory into the future, 
// as it has been set via RDMA
static void 
_future_set_no_copy(_netfuture_t *f) {
  //hpx_status_t status = cvar_get_error(&f->empty);
  f->bits ^= HPX_UNSET; // not empty anymore!
  f->bits |= HPX_SET;
  cvar_reset(&f->empty);
  scheduler_signal_all(&f->full);
}

static int
_future_set_no_copy_from_remote_action(_netfuture_t **fp) {
  _netfuture_t *f = *fp;
  dbg_printf("  _future_set_no_copy_from_remote_action on %p\n", (void*)f);
  lco_lock(&f->lco);
  if (!_empty(f))
    scheduler_wait(&f->lco.lock, &f->empty);
  _future_set_no_copy(f);
  lco_unlock(&f->lco);
  dbg_printf("  _future_set_no_copy_from_remote_action on %p DONE\n", (void*)f);
  return HPX_SUCCESS;
}

static int
_progress_action(void *args) {
  int flag;
  photon_rid request;
  //  int send_rank = -1;
  //  int i = 0;
  while (!shutdown) {
    // check send completion
    photon_probe_completion(PHOTON_ANY_SOURCE, &flag, &request, PHOTON_PROBE_EVQ);
    if (flag > 0) {
      dbg_printf("  Received send completion on rank %d for %" PRIx64 "\n", hpx_get_my_rank(), request);
      sync_fadd(&_outstanding_sends, -1, SYNC_RELEASE);
      if (request != 0){
	lco_t *lco = (lco_t*)request;
	lco_future_set(lco, 0, NULL);
      } 
    }

    photon_probe_completion(PHOTON_ANY_SOURCE, &flag, &request, PHOTON_PROBE_LEDGER);
    if (flag && request != 0) {
      dbg_printf("  Received recv completion on rank %d for future at %" PRIx64 "\n", hpx_get_my_rank(), request);
      _netfuture_t *f = (_netfuture_t*)request;
      lco_lock(&f->lco);
      
      // do set stuff
      if (!_empty(f)) {
        lco_unlock(&f->lco);
	hpx_call(HPX_HERE, _future_set_no_copy_from_remote, &f, sizeof(f), HPX_NULL);
      } else {
	_future_set_no_copy(f);
        lco_unlock(&f->lco);
      }
    } // end if
    /*
    i = (i + 1) % YIELD_COUNT;
    if (i == 0)
      hpx_thread_yield();
    */
    hpx_thread_yield();
  }
  return HPX_SUCCESS;
}

static void
_table_lock() {
  sync_lockable_ptr_lock(&_netfuture_table.lock);
}

static void
_table_unlock() {
  sync_lockable_ptr_unlock(&_netfuture_table.lock);
}

static int 
_initialize_netfutures_action(hpx_addr_t *ag) {
  _outstanding_send_limit = here->transport->get_send_limit(here->transport);
  //_outstanding_send_limit = 1;

  //pwc_lock = malloc(sizeof(*pwc_lock) * HPX_LOCALITIES);
  //assert(pwc_lock);
  
  dbg_printf("  Initializing futures on rank %d\n", hpx_get_my_rank());
  _table_lock();
  _netfuture_table.curr_index = 0;
  _netfuture_table.curr_capacity = _netfuture_cfg.total_number;
  _netfuture_table.curr_offset = 0;
  _netfuture_table.buffers = calloc(hpx_get_num_ranks(), sizeof(struct photon_buffer_t));
  _netfuture_table.fut_infos = calloc(_netfuture_table.curr_capacity, sizeof(_fut_info_t)) ;
  /*
  _netfuture_table.base = malloc(_netfuture_cfg.total_size);
  photon_register_buffer(_netfuture_table.base, _netfuture_cfg.total_size);
  */
  _netfuture_table.mem_size = _netfuture_cfg.total_size;
  _netfuture_table.base_gas = hpx_gas_alloc(_netfuture_cfg.total_size);
  dbg_printf("GAS base = 0x%"PRIx64".\n", _netfuture_table.base_gas);
  assert(hpx_gas_try_pin(_netfuture_table.base_gas, &_netfuture_table.base));

  memset(_netfuture_table.base, -1, _netfuture_cfg.total_size);

  struct  photon_buffer_t buffer;
  buffer.addr = (uintptr_t)_netfuture_table.base;

  //photon_get_buffer_private(_netfuture_table.base, _netfuture_cfg.total_size, &buffer.priv);
  
  dbg_printf("  At %d buffer = %p\n", hpx_get_my_rank(), _netfuture_table.base);
  
  hpx_lco_allgather_setid(*ag, hpx_get_my_rank(), 
  			  sizeof(struct photon_buffer_t), &buffer,
  			  HPX_NULL, HPX_NULL);
  
  hpx_lco_get(*ag, hpx_get_num_ranks() * sizeof(struct photon_buffer_t), _netfuture_table.buffers);

#if 0
  for (int i = 0; i < hpx_get_num_ranks(); i++) {
    dbg_printf("  At rank %d, buffer[%d].priv = %"PRIx64",%"PRIx64"\n", hpx_get_my_rank(), i, _netfuture_table.buffers[i].priv.key0,  _netfuture_table.buffers[i].priv.key1);
  }
#endif
  
  for (int i = 0; i < hpx_get_num_ranks(); i++) {
    transport_class_t *transport = here->transport;
    memcpy(&_netfuture_table.buffers[i].priv, &transport->rkey_table[i].rkey, sizeof(_netfuture_table.buffers[i].priv));
    //    _netfuture_table.buffers[i].priv.key1 = (uint64_6)transport->rkey_table[i];

    dbg_printf("  At rank %d, buffer[%d].priv = %"PRIx64",%"PRIx64"\n", hpx_get_my_rank(), i, _netfuture_table.buffers[i].priv.key0,  _netfuture_table.buffers[i].priv.key1);
  }

  _netfuture_table.inited = 1;

  hpx_call_async(HPX_HERE, _progress, NULL, 0, HPX_NULL, HPX_NULL);

  _table_unlock();
  dbg_printf("  Initialized futures on rank %d\n", hpx_get_my_rank());
  return HPX_SUCCESS;
}
  
void hpx_netfutures_fini() {
  shutdown = true;
}

hpx_status_t hpx_netfutures_init(hpx_netfuture_config_t *cfg) {
  if (cfg != NULL) {
    assert(cfg->total_size != 0);
    assert(cfg->total_number != 0);
    _netfuture_cfg.total_size = cfg->total_size;
    _netfuture_cfg.total_number = cfg->total_number;
  }
  
  hpx_addr_t ag = hpx_lco_allgather_new(hpx_get_num_ranks(), sizeof(struct photon_buffer_t));
  if (hpx_get_my_rank() != 0)
    return HPX_ERROR;
  hpx_addr_t done = hpx_lco_and_new(hpx_get_num_ranks());
  for (int i = 0; i < hpx_get_num_ranks(); i++)
    hpx_call(HPX_THERE(i), _initialize_netfutures, &ag, sizeof(hpx_addr_t), done);
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

  if (from && _is_inplace(f)) {
    memcpy(&f->data, from, size);
  }
  else if (from) {
    //    void *ptr = NULL;
    //    memcpy(&ptr, &f->data, sizeof(ptr));       // strict aliasing
    //    memcpy(ptr, from, size);
    memcpy(f->data, from, size);
  }

  f->bits ^= HPX_UNSET; // not empty anymore!
  f->bits |= HPX_SET;
  cvar_reset(&f->empty);
  scheduler_signal_all(&f->full);
 unlock:
  lco_unlock(&f->lco);
}

static void
_future_reset(lco_t *lco)
{
  _netfuture_t *f = (_netfuture_t *)lco;
  lco_lock(&f->lco);

  cvar_reset(&f->full);
  scheduler_signal_all(&f->empty);

  lco_unlock(&f->lco);
}

static int
_future_reset_remote_action(void *data)
{
  hpx_addr_t target = hpx_thread_current_target();
  lco_t *lco = NULL;
  if (!hpx_gas_try_pin(target, (void**)&lco))
    return HPX_RESEND;
  _future_reset(lco);

  hpx_gas_unpin(target);
  return HPX_SUCCESS;
}

/// Copies the appropriate value into @p out, waiting if the lco isn't set yet.
static hpx_status_t
_future_get(lco_t *lco, int size, void *out, bool set_empty)
{
  _netfuture_t *f = (_netfuture_t *)lco;
  lco_lock(&f->lco);

  hpx_status_t status = _wait(f);

  if (!_full(f))
    scheduler_wait(&f->lco.lock, &f->full);

  if (status != HPX_SUCCESS)
    goto unlock;

  if (out && _is_inplace(f)) {
    memcpy(out, &f->data, size);
    goto unlock;
  }

  if (out) {
    //    void *ptr = NULL;
    //    memcpy(&ptr, &f->data, sizeof(ptr));       // strict aliasing
    memcpy(out, f->data, size);
  }

  if (set_empty) {
    f->bits ^= HPX_SET;
    f->bits |= HPX_UNSET;
    cvar_reset(&f->full);
    scheduler_signal_all(&f->empty);
  }

 unlock:
  lco_unlock(&f->lco);
  return status;
}

static hpx_status_t
_future_wait_local(struct _future_wait_args *args)
{
  hpx_status_t status;
  _netfuture_t *f = args->fut;

  lco_lock(&f->lco);

  if (args->set == HPX_SET) {
    if (!_full(f))
      status = scheduler_wait(&f->lco.lock, &f->full);
    else
      status = cvar_get_error(&f->full);
    f->bits ^= HPX_SET;
    f->bits |= HPX_UNSET;
    cvar_reset(&f->full);
    scheduler_signal_all(&f->empty);
  }
  else {
    if (!_empty(f))
      status = scheduler_wait(&f->lco.lock, &f->empty);
    else
      status = cvar_get_error(&f->empty);
    /*
      f->bits ^= HPX_UNSET;
      f->bits |= HPX_SET;
      cvar_reset(&f->empty);
      scheduler_signal_all(&f->full);
    */
  }

  lco_unlock(&f->lco);

  return status;
}

static hpx_status_t
_future_wait_remote_action(struct _future_wait_args *args)
{
  return _future_wait_local(args);
}

/// initialize the future
static void
_future_init(_netfuture_t *f, int size, bool shared)
{
  // the future vtable
  static const lco_class_t vtable = {
    NULL,
    NULL,
    NULL,
    NULL, 
    NULL
  };

  bool inplace = false;
  lco_init(&f->lco, &vtable, inplace);
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
  _netfuture_table.fut_infos[f->table_index].offset = f->base_offset;

  _netfuture_table.curr_offset += (_netfutures_at_rank(f)) * f->size;
  _netfuture_table.curr_index++;
  return HPX_SUCCESS;
}

int _add_futures(hpx_netfuture_t *f) {
    hpx_netfuture_t fi = hpx_lco_netfuture_at(*f, hpx_get_my_rank());
    //    _netfuture_t *nf = (_netfuture_t*)_netfuture_get_addr(&fi) + (sizeof(_netfuture_t) + fi.size) * i;
  for (int i = 0; i < _netfutures_at_rank(f); i ++) {
    _netfuture_t *nf = (_netfuture_t*)_netfuture_get_addr(&fi);
    _future_init(nf, f->size, false);
    dbg_printf("  Initing future on rank %d at %p\n", hpx_get_my_rank(), (void*)nf);
    fi.index += hpx_get_num_ranks();
  }
  fi.index = 0;
  return HPX_SUCCESS;
}

static int
_add_future_to_table_action(hpx_netfuture_t *f) {
  _table_lock();
  hpx_status_t status = _update_table(f);

  _add_futures(f);

  _table_unlock();
  return status;
}

hpx_netfuture_t
hpx_lco_netfuture_new_all(int n, size_t size) {
  hpx_netfuture_t f;

    assert(_netfuture_table.curr_offset + n * (size + sizeof(_netfuture_t)) 
	            <  _netfuture_cfg.total_size * hpx_get_num_ranks());
    assert(_netfuture_table.curr_index + 1 < _netfuture_cfg.total_number);



  _table_lock();

  f.base_offset = _netfuture_table.curr_offset;
  f.table_index = _netfuture_table.curr_index;
  f.index = 0;
  f.size = size;
  f.count = n;

  _update_table(&f);

  _add_futures(&f);

  _table_unlock();

  hpx_addr_t done = hpx_lco_and_new(hpx_get_num_ranks() - 1);
  for (int i = 1; i < hpx_get_num_ranks(); i++)
    hpx_call(HPX_THERE(i), _add_future_to_table, &f, sizeof(f), done);

  hpx_lco_wait(done);

  dbg_printf("Done initing futures.");

  return f;
}

hpx_netfuture_t hpx_lco_netfuture_shared_new(size_t size) {
  // TODO
  //  return _netfuture_new(size, true);
  hpx_netfuture_t fut;
  return fut;
}

hpx_netfuture_t hpx_lco_netfuture_shared_new_all(int num_participants, size_t size) {
  // TODO
  //  return _new_all(num_participants, size, true);
  hpx_netfuture_t fut;
  return fut;
}

// Application level programmer doesn't know how big the future is, so we
// provide this array indexer.
hpx_netfuture_t 
hpx_lco_netfuture_at(hpx_netfuture_t array, int i) {
  assert(i >= 0 && i <= array.count);
  //  return &array[i];
  //return &_netfuture_table.futs[array->table_index][i];
  hpx_netfuture_t fut = array;
  fut.index = i;
  return fut;
}

static void
_put_with_completion(hpx_netfuture_t *future,  int id, size_t size, void *data,
			  hpx_addr_t lsync_lco, hpx_addr_t rsync_lco) {

  uint32_t old, new;
  old = _outstanding_send_limit;
  do {
    while (old >= _outstanding_send_limit)
      old = sync_load(&_outstanding_sends, SYNC_SEQ_CST);
    new = old + 1;
  } while (!sync_cas(&_outstanding_sends, old, new, SYNC_ACQ_REL, SYNC_RELAXED));

  // need the following information:
  
  // need to convey the following information:
  // local_rid to whomever is waiting on it locally
  // remote_rid to same

  // remote_rid can make the same as the identifier for the future being set
  // local_rid can be the same?

  hpx_netfuture_t *f = future;
  struct photon_buffer_t *buffer = &_netfuture_table.buffers[_netfuture_get_rank(f)];

  int remote_rank = _netfuture_get_rank(future);
  assert(remote_rank == id % hpx_get_num_ranks()); // TODO take out when base not 0 or refactored
  void *remote_ptr = (void*)_netfuture_get_data_addr(future);
  struct photon_buffer_priv_t remote_priv = buffer->priv;
  
  photon_rid local_rid;

  if (lsync_lco == HPX_NULL)
    local_rid = PHOTON_NOWAIT_TAG; // TODO if lsync != NULL use local_rid to represent local LCOs address
  else {
    void *ptr;
    hpx_gas_try_pin(lsync_lco, &ptr);
    local_rid = (photon_rid)ptr;
  }
  photon_rid remote_rid = _netfuture_get_addr(future);
  dbg_printf("  pwc with at %d for %d remote_rid == %" PRIx64 "\n", hpx_get_my_rank(), remote_rank, remote_rid);

  sync_lockable_ptr_lock(&pwc_lock);
  photon_put_with_completion(remote_rank, data, size, remote_ptr, remote_priv, 
			     local_rid, remote_rid, PHOTON_REQ_ONE_CQE);
  sync_lockable_ptr_unlock(&pwc_lock);
  
  // TODO add to queue for threads to check

  if (!hpx_addr_eq(lsync_lco, HPX_NULL)) {
    // wait at shared netfuture while value != local_rid
  }
  if (!hpx_addr_eq(rsync_lco, HPX_NULL)) {
    // wait at shared netfuture while value != remote_rid
  }
}

void hpx_lco_netfuture_setat(hpx_netfuture_t future, int id, size_t size, hpx_addr_t value,
			     hpx_addr_t lsync_lco, hpx_addr_t rsync_lco) {

  assert(id >= 0 && id <= future.count);
  void *data;
  assert(hpx_gas_try_pin(value, &data)); // TODO: unpin this

  hpx_netfuture_t future_i = hpx_lco_netfuture_at(future, id);

  dbg_printf("  Setating to %d (%d, future at %p) from %d\n", future_i.index, _netfuture_get_rank(&future_i), (void*)_netfuture_get_addr(&future_i), hpx_get_my_rank());

  //  dbg_printf("  Putting to (%d, %p) from %d\n", _netfuture_get_rank(future_i), (void*)future_i->buffer.addr, hpx_get_my_rank());
  
  // normally lco_set does all this
  if (_netfuture_get_rank(&future_i) != hpx_get_my_rank()) {
    _put_with_completion(&future_i, id, size, data, lsync_lco, rsync_lco);
  }
  else {
    _future_set_with_copy((_netfuture_t*)_netfuture_get_addr(&future_i), size, data);  
  if (!(hpx_addr_eq(lsync_lco, HPX_NULL)))
    hpx_lco_set(lsync_lco, 0, NULL, HPX_NULL, HPX_NULL);
  if (!(hpx_addr_eq(rsync_lco, HPX_NULL)))
    hpx_lco_set(rsync_lco, 0, NULL, HPX_NULL, HPX_NULL);
  }

  dbg_printf("  Done setting to (%d, %p) from %d\n", _netfuture_get_rank(&future_i), (void*)_netfuture_get_addr(&future_i), hpx_get_my_rank());
}

void hpx_lco_netfuture_emptyat(hpx_netfuture_t base, int i, hpx_addr_t rsync_lco) {
  hpx_netfuture_t future_i = hpx_lco_netfuture_at(base, i);

  assert(i >= 0 && i <= base.count);
  assert(_netfuture_get_rank(&future_i) == hpx_get_my_rank());

  _netfuture_t *f = (_netfuture_t*)_netfuture_get_addr(&future_i);

  assert(_full(f));

  lco_lock(&f->lco);
  
  f->bits ^= HPX_SET;
  f->bits |= HPX_UNSET;
  cvar_reset(&f->full);
  scheduler_signal_all(&f->empty);

  lco_unlock(&f->lco);
}

hpx_addr_t hpx_lco_netfuture_getat(hpx_netfuture_t base, int i, size_t size) {
  assert(i >= 0 && i <= base.count);

  hpx_addr_t retval = HPX_NULL;
  hpx_netfuture_t future_i = hpx_lco_netfuture_at(base, i);

  lco_t *lco;

  dbg_printf("  Getating %d from (%d, future at %p) to %d\n", future_i.index, _netfuture_get_rank(&future_i), (void*)_netfuture_get_addr(&future_i), hpx_get_my_rank());

  assert(_netfuture_get_rank(&future_i) == hpx_get_my_rank());
  
  lco = (lco_t*)_netfuture_get_addr(&future_i);
  retval = hpx_addr_add(_netfuture_table.base_gas, _netfuture_get_offset(&future_i) + sizeof(_netfuture_t), 1);
  //  hpx_addr_t out_gas = hpx_gas_alloc(size);
  //  retval = out_gas;
  //  void *out;
  //  assert(hpx_gas_try_pin(out_gas, &out));
  _future_get(lco, size, NULL, false);
  //  hpx_gas_unpin(out_gas);
  dbg_printf("  Done getting from (%d, %p) to %d\n", _netfuture_get_rank(&future_i), (void*)_netfuture_get_addr(&future_i), hpx_get_my_rank());
  return retval;
}

// this is a highly suboptimal implementation
// ideally this would be done more like wait_all is implemented
void hpx_lco_netfuture_get_all(size_t num, hpx_netfuture_t futures, size_t size,
			       void *values[]) {
  // TODO
}

void hpx_lco_netfuture_waitat(hpx_netfuture_t future, int id, hpx_set_t set) {
#if 0
  hpx_netfuture_t future_i = hpx_lco_netfuture_at(future, id);

  _netfuture_t *fut = (_netfuture_t*)_netfuture_get_addr(future_i);

  struct _future_wait_args args;
  args.fut = fut;
  args.set = set;

  if (_netfuture_get_rank(future_i) != hpx_get_my_rank()) {
    hpx_addr_t done = hpx_lco_future_new(0);
    hpx_call_async(HPX_THERE(_netfuture_get_rank(future_i)), _future_wait_remote, &args, sizeof(args), HPX_NULL, done);
    hpx_lco_wait(done);
    return;
  }
  
  //  hpx_call_sync(HPX_HERE, _future_wait_remote, &args, sizeof(args), NULL, 0);
  _future_wait_local(&args);
#endif
}

hpx_status_t hpx_lco_netfuture_waitat_for(hpx_netfuture_t future, int id, hpx_set_t set, hpx_time_t time) {
  return HPX_ERROR;
}

hpx_status_t hpx_lco_netfuture_waitat_until(hpx_netfuture_t future, int id, hpx_set_t set, hpx_time_t time) {
  return HPX_ERROR;
}

void hpx_lco_netfuture_wait_all(size_t num, hpx_netfuture_t netfutures, hpx_set_t set) {
#if 0
  hpx_addr_t done = hpx_lco_and_new(num);
  struct _future_wait_args *args = malloc(sizeof(args));
  args->set = set;

  for (int i = 0; i < num; i++) {
    hpx_netfuture_t target = hpx_lco_netfuture_at(netfutures, i);
    hpx_call_async(HPX_THERE(_netfuture_get_rank(target)), _future_wait_remote, args, sizeof(args), HPX_NULL, done);
  }

  hpx_lco_wait(done);
  free(args);

  return;
#endif
}

hpx_status_t hpx_lco_netfuture_wait_all_for(size_t num, hpx_netfuture_t netfutures, 
					    hpx_set_t set, hpx_time_t time) {
  return HPX_ERROR;
}

hpx_status_t hpx_lco_netfuture_wait_all_until(size_t num, hpx_netfuture_t netfutures, 
					    hpx_set_t set, hpx_time_t time) {
  return HPX_ERROR;
}

void hpx_lco_netfuture_free(hpx_netfuture_t future) {
  // TODO

}


void hpx_lco_netfuture_free_all(int num, hpx_netfuture_t base) {
  // Ideally this would not need to know the number of futures. But in order to avoid that
  // we would need to add to futures that are created and deleted with _all() functions
  // a pointer to the base future and a count.

  // TODO
}

bool hpx_lco_netfuture_is_shared(hpx_netfuture_t target) {
  return false;
}

int
hpx_lco_netfuture_get_rank(hpx_netfuture_t future) {
  return _netfuture_get_rank(&future);
}

static void HPX_CONSTRUCTOR
_future_initialize_actions(void) {
  _future_reset_remote = HPX_REGISTER_ACTION(_future_reset_remote_action);
  _future_wait_remote = HPX_REGISTER_ACTION(_future_wait_remote_action);

  _is_shared = HPX_REGISTER_ACTION(_is_shared_action);
  _future_set_no_copy_from_remote = HPX_REGISTER_ACTION(_future_set_no_copy_from_remote_action);

  _progress = HPX_REGISTER_ACTION(_progress_action);
  _add_future_to_table = HPX_REGISTER_ACTION(_add_future_to_table_action);
  _initialize_netfutures = HPX_REGISTER_ACTION(_initialize_netfutures_action);
}
