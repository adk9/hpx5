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
#include "lco.h"
#include "cvar.h"

#define _NEWFUTURES_MEMORY_DEFAULT 1024*1024*1024
#define _NEWFUTURES_CAPACITY_DEFAULT 10000
#define PHOTON_NOWAIT_TAG 0
#define FT_SHARED 1<<3
static const int _NEWFUTURE_EXCHG = -37;

typedef struct {
  lco_t lco;
  cvar_t full;
  cvar_t empty;
  uint32_t bits;
  void *value;
  int home_rank;
  void* home_address;
  char data[];
} _newfuture_t;

struct _future_wait_args {
  _newfuture_t *fut;
  hpx_set_t set;
  hpx_time_t time;
};

static hpx_action_t _is_shared = 0;

/// Remote block initialization
static hpx_action_t _future_reset_remote = 0;
static hpx_action_t _future_wait_remote = 0;

static hpx_action_t _future_set_no_copy_from_remote = 0;
static hpx_action_t _recv_queue_progress = 0;
static hpx_action_t _send_queue_progress = 0;
static hpx_action_t _add_future_to_table = 0;
static hpx_action_t _initialize_newfutures = 0;

static bool _empty(const _newfuture_t *f) {
  return f->bits & HPX_UNSET;
}

static bool _full(const _newfuture_t *f) {
  return f->bits & HPX_SET;
}

/// Use the LCO's "user" state to remember if the future is in-place or not.
static uintptr_t
_is_inplace(const _newfuture_t *f) {
  return lco_get_user(&f->lco);
}

static int
_is_shared_action(void* args) {
  _newfuture_t *fut;
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
  
  size_t ghost_offset; // sync add

  // must be locked for write only
  struct photon_buffer_t *buffers; // one for each rank
  void* base;
  void *ghosts_base;


  _fut_info_t *fut_infos;
  
} _newfuture_table_t;

static _newfuture_table_t _newfuture_table = {.inited = 0};

// which rank is the future on?
static int 
_newfuture_get_rank(hpx_newfuture_t *f) {
  return f->index % hpx_get_num_ranks(); // TODO change if we want to allow base_rank != 0
}

static int
_newfutures_at_rank(hpx_newfuture_t *f) {
  return (f->count + hpx_get_num_ranks() - 1)/hpx_get_num_ranks();
}

static size_t
_newfuture_get_offset(hpx_newfuture_t *f) {
  size_t size = sizeof(_newfuture_t) + f->size;
  return _newfuture_table.fut_infos[f->table_index].offset + (size * (f->index % hpx_get_num_ranks()));
}

// the native address of the _newfuture_t representation of a future
static uintptr_t 
_newfuture_get_addr(hpx_newfuture_t *f) {
  uintptr_t offset =  _newfuture_get_offset(f);
  int rank = _newfuture_get_rank(f);
  uintptr_t rank_base = _newfuture_table.buffers[rank].addr;
  return rank_base + offset;
}

// the native address of the _newfuture_t representation of the future's data
static uintptr_t 
_newfuture_get_data_addr(hpx_newfuture_t *f) {
  return _newfuture_get_addr(f) + sizeof(_newfuture_t);
}

static int
_send_queue_progress_action(void* args) {
  int flag;
  photon_rid request;
  int rc;
  while (1) {
    rc = photon_probe_completion(PHOTON_ANY_SOURCE, &flag, &request, PHOTON_PROBE_EVQ);
    if (flag > 0) {
      printf("Received send completion %" PRIx64 "\n", request);
    }
    if (rc < 0) {
      
    }
    if ((flag > 0) && (request == PHOTON_NOWAIT_TAG)) {
	
    }
    hpx_thread_yield();
  }
  return HPX_SUCCESS;
}

// this is what we use when we do NOT need to copy memory into the future, 
// as it has been set via RDMA
static void 
_future_set_no_copy(_newfuture_t *f) {
  //hpx_status_t status = cvar_get_error(&f->empty);
  f->bits ^= HPX_UNSET; // not empty anymore!
  f->bits |= HPX_SET;
  cvar_reset(&f->empty);
  scheduler_signal_all(&f->full);
}

static int
_future_set_no_copy_from_remote_action(_newfuture_t **fp) {
  _newfuture_t *f = *fp;
  printf("_future_set_no_copy_from_remote_action on %p\n", (void*)f);
  lco_lock(&f->lco);
  if (!_empty(f))
    scheduler_wait(&f->lco.lock, &f->empty);
  _future_set_no_copy(f);
  lco_unlock(&f->lco);
  return HPX_SUCCESS;
}

static int
_recv_queue_progress_action(void *args) {
  int flag;
  photon_rid request;
  //  int send_rank = -1;
  do {
    /*
    send_rank++;
    send_rank = send_rank % hpx_get_num_ranks();
    if (send_rank == hpx_get_my_rank())
      continue;
    */
    // you want to get completions from any source, even yourself
    photon_probe_completion(PHOTON_ANY_SOURCE, &flag, &request, PHOTON_PROBE_LEDGER);
    if (flag > 0) {
      printf("Received recv completion %" PRIx64 "\n", request);
    }
    if (flag && request != 0) {
      _newfuture_t *f = (_newfuture_t*)request;
      lco_lock(&f->lco);
      
      // do set stuff
      if (!_empty(f))
	hpx_call_async(HPX_HERE, _future_set_no_copy_from_remote, &f, sizeof(&f), HPX_NULL, HPX_NULL);
      else {
	_future_set_no_copy(f);
      }
      lco_unlock(&f->lco);
    } // end if
    hpx_thread_yield();
  } while (1);
  return HPX_SUCCESS;
}

static void
_table_lock() {
  sync_lockable_ptr_lock(&_newfuture_table.lock);
}

static void
_table_unlock() {
  sync_lockable_ptr_unlock(&_newfuture_table.lock);
}

static int 
_initialize_newfutures_action(hpx_addr_t *ag) {
  printf("Initializing futures on rank %d\n", hpx_get_my_rank());
  _table_lock();
  _newfuture_table.curr_index = 0;
  _newfuture_table.curr_capacity = _NEWFUTURES_CAPACITY_DEFAULT;
  _newfuture_table.curr_offset = 0;
  _newfuture_table.ghost_offset = 0;
  _newfuture_table.buffers = calloc(hpx_get_num_ranks(), sizeof(struct photon_buffer_t));
  _newfuture_table.fut_infos = calloc(_newfuture_table.curr_capacity, sizeof(_fut_info_t)) ;
  _newfuture_table.base = malloc(_NEWFUTURES_MEMORY_DEFAULT);
  photon_register_buffer(_newfuture_table.base, _NEWFUTURES_MEMORY_DEFAULT);
  _newfuture_table.ghosts_base = malloc(_NEWFUTURES_MEMORY_DEFAULT);
  photon_register_buffer(_newfuture_table.ghosts_base, _NEWFUTURES_MEMORY_DEFAULT);


  struct  photon_buffer_t buffer;
  buffer.addr = (uintptr_t)_newfuture_table.base;
  photon_get_buffer_private(_newfuture_table.base, _NEWFUTURES_MEMORY_DEFAULT, &buffer.priv);

  printf("At %d buffer = %p\n", hpx_get_my_rank(), _newfuture_table.base);

  hpx_lco_allgather_setid(*ag, hpx_get_my_rank(), 
			  sizeof(struct photon_buffer_t), &buffer,
			  HPX_NULL, HPX_NULL);
  
  hpx_lco_get(*ag, hpx_get_num_ranks() * sizeof(struct photon_buffer_t), _newfuture_table.buffers);

  _newfuture_table.inited = 1;

  hpx_call_async(HPX_HERE, _recv_queue_progress, NULL, 0, HPX_NULL, HPX_NULL);
  hpx_call_async(HPX_HERE, _send_queue_progress, NULL, 0, HPX_NULL, HPX_NULL);

  _table_unlock();
  printf("Initialized futures on rank %d\n", hpx_get_my_rank());
  return HPX_SUCCESS;
}

hpx_status_t hpx_newfutures_init() {
  hpx_addr_t ag = hpx_lco_allgather_new(hpx_get_num_ranks(), sizeof(struct photon_buffer_t));
  if (hpx_get_my_rank() != 0)
    return HPX_ERROR;
  hpx_addr_t done = hpx_lco_and_new(hpx_get_num_ranks());
  for (int i = 0; i < hpx_get_num_ranks(); i++)
    hpx_call(HPX_THERE(i), _initialize_newfutures, &ag, sizeof(hpx_addr_t), done);
  hpx_lco_wait(done);
  return HPX_SUCCESS;
}

static hpx_status_t
_wait(_newfuture_t *f) {
  if (!_full(f))
    return scheduler_wait(&f->lco.lock, &f->full);
  else
    return cvar_get_error(&f->full);
}

static void
_free(_newfuture_t *f) {
}

// this version is for when we need to copy memory
static void
_future_set_with_copy(lco_t *lco, int size, const void *from)
{
  _newfuture_t *f = (_newfuture_t *)lco;
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
    memcpy(&f->value, from, size);
  }
  else if (from) {
    void *ptr = NULL;
    memcpy(&ptr, &f->value, sizeof(ptr));       // strict aliasing
    memcpy(ptr, from, size);
  }

  uint64_t buffer;
  if (_is_inplace(f))
    memcpy(&buffer, &f->value, 4);
  else
    memcpy(&buffer, f->value, 4);

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
  _newfuture_t *f = (_newfuture_t *)lco;
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

static void
_future_error(lco_t *lco, hpx_status_t code)
{
  _newfuture_t *f = (_newfuture_t *)lco;
  lco_lock(&f->lco);
  scheduler_signal_error(&f->full, code);
  lco_unlock(&f->lco);
}


/// Copies the appropriate value into @p out, waiting if the lco isn't set yet.
static hpx_status_t
_future_get(lco_t *lco, int size, void *out)
{
  _newfuture_t *f = (_newfuture_t *)lco;
  lco_lock(&f->lco);

  hpx_status_t status = _wait(f);

  if (!_full(f))
    scheduler_wait(&f->lco.lock, &f->full);

  if (status != HPX_SUCCESS)
    goto unlock;

  if (out && _is_inplace(f)) {
    memcpy(out, &f->value, size);
    goto unlock;
  }

  if (out) {
    void *ptr = NULL;
    memcpy(&ptr, &f->value, sizeof(ptr));       // strict aliasing
    memcpy(out, ptr, size);
  }

  if ((f->bits | FT_SHARED) == false) { // shared futures must be cleared manually
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
_future_wait(lco_t *lco)
{
  _newfuture_t *f = (_newfuture_t *)lco;
  lco_lock(&f->lco);
  hpx_status_t status = _wait(f);
  lco_unlock(&f->lco);
  return status;
}

static hpx_status_t
_future_wait_local(struct _future_wait_args *args)
{
  hpx_status_t status;
  _newfuture_t *f = args->fut;

  lco_lock(&f->lco);
  
  if (args->set == HPX_SET) {
    if (!_full(f))
      status = scheduler_wait(&f->lco.lock, &f->full);
    else
      status = cvar_get_error(&f->full);
    if ((f->bits | FT_SHARED) == false) {
      f->bits ^= HPX_SET;
      f->bits |= HPX_UNSET;
      cvar_reset(&f->full);
      scheduler_signal_all(&f->empty);
    }
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
_future_init(_newfuture_t *f, int size, bool shared)
{
  // the future vtable
  static const lco_class_t vtable = {
    NULL,
    _future_error,
    _future_set_with_copy,
    _future_get,
    _future_wait
  };

  bool inplace = (size <= sizeof(f->value));
  lco_init(&f->lco, &vtable, inplace);
  cvar_reset(&f->empty);
  cvar_reset(&f->full);
  f->bits = 0 | HPX_UNSET; // future starts out empty
  if (shared)
    f->bits |= FT_SHARED;
  if (!inplace) {
    f->value = malloc(size);                    // allocate if necessary
    assert(f->value);
  }
  else {
    memset(&f->value, 0, sizeof(f->value));
  }
}

hpx_newfuture_t 
hpx_lco_newfuture_new(size_t size) {
  return hpx_lco_newfuture_new_all(1, size);
  //return _newfuture_new(size, false);
}

struct new_all_args {
  int n;
  int base_rank;
  size_t size;
  hpx_addr_t allg;
};

int update_table(hpx_newfuture_t *f) {
  _newfuture_table.fut_infos[f->table_index].table_index = f->table_index;
  _newfuture_table.fut_infos[f->table_index].offset = f->base_offset;

  _newfuture_table.curr_offset += (_newfutures_at_rank(f)) * f->size;
  _newfuture_table.curr_index++;
  return HPX_SUCCESS;
}

static int
_add_future_to_table_action(hpx_newfuture_t *f) {
  _table_lock();
  hpx_status_t status = update_table(f);

  for (int i = 0; i < _newfutures_at_rank(f); i ++) {
    hpx_newfuture_t fi = hpx_lco_newfuture_at(*f, hpx_get_my_rank());
    _newfuture_t *nf = (_newfuture_t*)_newfuture_get_addr(&fi) + (sizeof(_newfuture_t) + fi.size) * i;
    _future_init(nf, f->size, false);
  }

  _table_unlock();
  return status;
}

hpx_newfuture_t
hpx_lco_newfuture_new_all(int n, size_t size) {
  hpx_newfuture_t f;

  _table_lock();

  f.base_offset = _newfuture_table.curr_offset;
  f.table_index = _newfuture_table.curr_index;
  f.index = 0;
  f.size = size;
  f.count = n;

  update_table(&f);

  _table_unlock();

  hpx_addr_t done = hpx_lco_and_new(hpx_get_num_ranks() - 1);
  for (int i = 1; i < hpx_get_num_ranks(); i++)
    hpx_call(HPX_THERE(i), _add_future_to_table, &f, sizeof(f), done);

  for (int i = 0; i < _newfutures_at_rank(&f); i ++) {
    hpx_newfuture_t fi = hpx_lco_newfuture_at(f, hpx_get_my_rank());
    _newfuture_t *nf = (_newfuture_t*)_newfuture_get_addr(&fi) + (sizeof(_newfuture_t) + size) * i;
    _future_init(nf, size, false);
  }

  hpx_lco_wait(done);

  return f;
}

hpx_newfuture_t hpx_lco_newfuture_shared_new(size_t size) {
  // TODO
  //  return _newfuture_new(size, true);
  hpx_newfuture_t fut;
  return fut;
}

hpx_newfuture_t hpx_lco_newfuture_shared_new_all(int num_participants, size_t size) {
  // TODO
  //  return _new_all(num_participants, size, true);
  hpx_newfuture_t fut;
  return fut;
}

// Application level programmer doesn't know how big the future is, so we
// provide this array indexer.
hpx_newfuture_t 
hpx_lco_newfuture_at(hpx_newfuture_t array, int i) {
  //  return &array[i];
  //return &_newfuture_table.futs[array->table_index][i];
  hpx_newfuture_t fut = array;
  fut.index = i;
  return fut;
}

static void
_put_with_completion(hpx_newfuture_t *future,  int id, size_t size, void *data,
			  hpx_addr_t lsync_lco, hpx_addr_t rsync_lco) {
  // need the following information:
  
  // need to convey the following information:
  // local_rid to whomever is waiting on it locally
  // remote_rid to same

  // remote_rid can make the same as the identifier for the future being set
  // local_rid can be the same?

  hpx_newfuture_t *f = future;
  struct photon_buffer_t *buffer = &_newfuture_table.buffers[_newfuture_get_rank(f)];

  int remote_rank = _newfuture_get_rank(future);
  void *remote_ptr = (void*)_newfuture_get_data_addr(future);
  struct photon_buffer_priv_t remote_priv = buffer->priv;
  photon_rid local_rid = PHOTON_NOWAIT_TAG; // TODO if lsync != NULL use local_rid to represent local LCOs address
  photon_rid remote_rid = _newfuture_get_addr(future);
  photon_put_with_completion(remote_rank, data, size, remote_ptr, remote_priv, 
			     local_rid, remote_rid, PHOTON_REQ_ONE_CQE);
  
  // TODO add to queue for threads to check

  if (!hpx_addr_eq(lsync_lco, HPX_NULL)) {
    // wait at shared newfuture while value != local_rid
  }
  if (!hpx_addr_eq(rsync_lco, HPX_NULL)) {
    // wait at shared newfuture while value != remote_rid
  }
}

void hpx_lco_newfuture_setat(hpx_newfuture_t future, int id, size_t size, void *data,
			     hpx_addr_t lsync_lco, hpx_addr_t rsync_lco) {
  hpx_newfuture_t future_i = hpx_lco_newfuture_at(future, id);

  printf("Putting to (%d, %p) from %d\n", _newfuture_get_rank(&future_i), (void*)_newfuture_get_addr(&future_i), hpx_get_my_rank());

  //  printf("Putting to (%d, %p) from %d\n", _newfuture_get_rank(future_i), (void*)future_i->buffer.addr, hpx_get_my_rank());
  
  // normally lco_set does all this
  if (_newfuture_get_rank(&future_i) != hpx_get_my_rank()) {
    _put_with_completion(&future_i, id, size, data, lsync_lco, rsync_lco);
  }
  else
    _future_set_with_copy((lco_t*)_newfuture_get_addr(&future_i), size, data);  
}

void hpx_lco_newfuture_emptyat(hpx_newfuture_t base, int i, hpx_addr_t rsync_lco) {
}

hpx_status_t hpx_lco_newfuture_getat(hpx_newfuture_t base, int i, size_t size, void *value) {
  hpx_newfuture_t future_i = hpx_lco_newfuture_at(base, i);

  lco_t *lco;

  printf("Getting from (%d, %p) to %d\n", _newfuture_get_rank(&future_i), (void*)_newfuture_get_addr(&future_i), hpx_get_my_rank());

  if (_newfuture_get_rank(&future_i) != hpx_get_my_rank()) {
    return HPX_ERROR;
  }
  else {
    lco = (lco_t*)_newfuture_get_addr(&future_i);
  }
  return _future_get(lco, size, value);
}



// this is a highly suboptimal implementation
// ideally this would be done more like wait_all is implemented
void hpx_lco_newfuture_get_all(size_t num, hpx_newfuture_t futures, size_t size,
			       void *values[]) {
  // TODO
}

void hpx_lco_newfuture_waitat(hpx_newfuture_t future, int id, hpx_set_t set) {
#if 0
  hpx_newfuture_t future_i = hpx_lco_newfuture_at(future, id);

  _newfuture_t *fut = (_newfuture_t*)_newfuture_get_addr(future_i);

  struct _future_wait_args args;
  args.fut = fut;
  args.set = set;

  if (_newfuture_get_rank(future_i) != hpx_get_my_rank()) {
    hpx_addr_t done = hpx_lco_future_new(0);
    hpx_call_async(HPX_THERE(_newfuture_get_rank(future_i)), _future_wait_remote, &args, sizeof(args), HPX_NULL, done);
    hpx_lco_wait(done);
    return;
  }
  
  //  hpx_call_sync(HPX_HERE, _future_wait_remote, &args, sizeof(args), NULL, 0);
  _future_wait_local(&args);
#endif
}

hpx_status_t hpx_lco_newfuture_waitat_for(hpx_newfuture_t future, int id, hpx_set_t set, hpx_time_t time) {
  return HPX_ERROR;
}

hpx_status_t hpx_lco_newfuture_waitat_until(hpx_newfuture_t future, int id, hpx_set_t set, hpx_time_t time) {
  return HPX_ERROR;
}

void hpx_lco_newfuture_wait_all(size_t num, hpx_newfuture_t newfutures, hpx_set_t set) {
#if 0
  hpx_addr_t done = hpx_lco_and_new(num);
  struct _future_wait_args *args = malloc(sizeof(args));
  args->set = set;

  for (int i = 0; i < num; i++) {
    hpx_newfuture_t target = hpx_lco_newfuture_at(newfutures, i);
    hpx_call_async(HPX_THERE(_newfuture_get_rank(target)), _future_wait_remote, args, sizeof(args), HPX_NULL, done);
  }

  hpx_lco_wait(done);
  free(args);

  return;
#endif
}

hpx_status_t hpx_lco_newfuture_wait_all_for(size_t num, hpx_newfuture_t newfutures, 
					    hpx_set_t set, hpx_time_t time) {
  return HPX_ERROR;
}

hpx_status_t hpx_lco_newfuture_wait_all_until(size_t num, hpx_newfuture_t newfutures, 
					    hpx_set_t set, hpx_time_t time) {
  return HPX_ERROR;
}

void hpx_lco_newfuture_free(hpx_newfuture_t future) {
  // TODO

}


void hpx_lco_newfuture_free_all(int num, hpx_newfuture_t base) {
  // Ideally this would not need to know the number of futures. But in order to avoid that
  // we would need to add to futures that are created and deleted with _all() functions
  // a pointer to the base future and a count.

  // TODO
}

bool hpx_lco_newfuture_is_shared(hpx_newfuture_t target) {
  return false;
}

int
hpx_lco_newfuture_get_rank(hpx_newfuture_t future) {
  return _newfuture_get_rank(&future);
}

static void HPX_CONSTRUCTOR
_future_initialize_actions(void) {
  _future_reset_remote = HPX_REGISTER_ACTION(_future_reset_remote_action);
  _future_wait_remote = HPX_REGISTER_ACTION(_future_wait_remote_action);

  _is_shared = HPX_REGISTER_ACTION(_is_shared_action);
  _future_set_no_copy_from_remote = HPX_REGISTER_ACTION(_future_set_no_copy_from_remote_action);

  _recv_queue_progress = HPX_REGISTER_ACTION(_recv_queue_progress_action);
  _send_queue_progress = HPX_REGISTER_ACTION(_send_queue_progress_action);
  _add_future_to_table = HPX_REGISTER_ACTION(_add_future_to_table_action);
  _initialize_newfutures = HPX_REGISTER_ACTION(_initialize_newfutures_action);
}
