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
#include <stdlib.h>
#include <string.h>

#include <photon.h>

#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/scheduler.h"
#include "lco.h"
#include "cvar.h"

// ==============================================================
//
// New futures implementation
//
// ==============================================================

/*
// The folowing was part of the original design, but the actual implementation
// does not use these values.
/// FT_FREE is set to true if there are pre-allocated future description
#define FT_FREE     0x00
/// FT_EMPTY is set if the future container size is 0, false otherwise
#define FT_EMPTY    0x01
/// FT_FULL is set if the future container size is full, false otherwise
#define FT_FULL     0x03
/// Async is when the action is defined with the future, to be executed
/// asynchronously. The creater of the asynchronous operation can then use
/// a variety of methods to query waitfor, or extract a value from future.
/// These may block if the asynchronous operation has not yet provided a
/// value
#define FT_ASYNCH   0x05
/// Wait has a waiter already
#define FT_WAIT     0x09
/// Waits for the result. Gets set if it is not available for the specific
/// timeout duration.
#define FT_WAITFORA 0x0D
/// Waits for the result, gets set if result is not available until specified
/// time pount has been reached
#define FT_WAITUNTILA 0x0E
/// This state is set to true if *this refers to a shared state otherwise
/// false.
#define FT_SHARED   0x10
*/

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

enum  _future_wait_action { _future_wait_full, _future_wait_for, _future_wait_until };

struct _future_wait_args {
  _newfuture_t *fut;
  hpx_set_t set;
  enum _future_wait_action wait_action;
  hpx_time_t time;
};

/// Freelist allocation for futures.
static __thread _newfuture_t *_free_futures = NULL;

static hpx_action_t _is_shared = 0;

/// Remote block initialization
static hpx_action_t _future_block_init = 0;
static hpx_action_t _future_blocks_init = 0;
static hpx_action_t _future_reset_remote = 0;
static hpx_action_t _future_wait_remote = 0;

static hpx_action_t _future_set_no_copy_from_remote = 0;
static hpx_action_t _recv_queue_progress = 0;
static hpx_action_t _send_queue_progress = 0;
static hpx_action_t _new_all_remote = 0;

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


////////////
///// start hacks
///////////

typedef struct {
  lockable_ptr_t lock;
  int index;
  int capacity;
  hpx_newfuture_t **futs;
} _newfuture_table_t;

static _newfuture_table_t _newfuture_table;


#define PHOTON_NOWAIT_TAG 0

// which rank is the future on?
static int 
_newfuture_get_rank(hpx_newfuture_t *f) {
  return f->base_rank + (f->id % hpx_get_num_ranks());
}

// the native address of the _newfuture_t representation of a future
static uintptr_t 
_newfuture_get_addr(hpx_newfuture_t *f) {
  return (uintptr_t)f->buffer.addr + (f->id % hpx_get_num_ranks()) * f->size_per;
}

// the native address of the _newfuture_t representation of the future's data
static uintptr_t 
_newfuture_get_data_addr(hpx_newfuture_t *f) {
  return (uintptr_t)f->buffer.addr + (f->id % hpx_get_num_ranks()) * (sizeof(*f) + f->size_per) + f->size_per;
}

static int
_send_queue_progress_action(void* args) {
  int flag;
  photon_rid request;
  int rc;
  while (1) {
    rc = photon_probe_completion(PHOTON_ANY_SOURCE, &flag, &request, PHOTON_PROBE_EVQ);
    if (rc < 0) {
      
    }
    if ((flag > 0) && (request == PHOTON_NOWAIT_TAG)) {
	
    }
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
_future_set_no_copy_from_remote_action(void *args) {
  _newfuture_t *f = (_newfuture_t*)args;
  lco_lock(&f->lco);
  scheduler_wait(&f->lco.lock, &f->empty);
  _future_set_no_copy(f);
  lco_unlock(&f->lco);
  return HPX_SUCCESS;
}

static int
_recv_queue_progress_action(void *args) {
  int flag;
  photon_rid request;
  int send_rank = -1;
  do {
    send_rank++;
    send_rank = send_rank % hpx_get_num_ranks();
    if (send_rank != hpx_get_my_rank())
      continue;
    photon_probe_completion(send_rank, &flag, &request, PHOTON_PROBE_LEDGER);
    if (flag && request != 0) {
      _newfuture_t *f = (_newfuture_t*)request;
      lco_lock(&f->lco);
  
      // do set stuff
      if (!_empty(f))
	hpx_call_async(HPX_HERE, _future_set_no_copy_from_remote, f, sizeof(f), HPX_NULL, HPX_NULL);
      else {
	_future_set_no_copy(f);
      }
      lco_unlock(&f->lco);
    } // end if
  } while (1);
  return HPX_SUCCESS;
}

void initialize_newfutures() {
  _newfuture_table.index = 0;
  _newfuture_table.capacity = 1000;
  _newfuture_table.futs = malloc(sizeof(hpx_newfuture_t*) * _newfuture_table.capacity);

  // alltoall:
  // exchange private structures and remote addresses

  // local:
  // start thread(s)
  // - local completion
  // - remote completion for this sender?
  // - remote completion for each sender

  hpx_call_async(HPX_HERE, _recv_queue_progress, NULL, 0, HPX_NULL, HPX_NULL);
  hpx_call_async(HPX_HERE, _send_queue_progress, NULL, 0, HPX_NULL, HPX_NULL);

#if 0
  send_queue = sync_two_lock_queue_new();
  recv_queues = calloc(hpx_get_num_ranks(), sizeof(two_lock_queue_t*));
  for (int i = 0; i < hpx_get_num_ranks(); i++)
    recv_queues[i] = sync_two_lock_queue_new();
#endif



}
/////////
/// end main part of hacks
/////////

static hpx_status_t
_wait(_newfuture_t *f) {
  if (!_full(f))
    return scheduler_wait(&f->lco.lock, &f->full);
  else
    return cvar_get_error(&f->full);
}

static void
_free(_newfuture_t *f) {
  // overload the value for freelisting---not perfect
  f->value = _free_futures;
  _free_futures = f;
}


/// Deletes the future's out of place data, if necessary.
///
/// NB: deadlock issue here
static void
_future_fini(lco_t *lco)
{
  if (!lco)
    return;

  _newfuture_t *f = (_newfuture_t *)lco;
  lco_lock(&f->lco);

  if (!_is_inplace(f)) {
    void *ptr = NULL;
    memcpy(&ptr, &f->value, sizeof(ptr));       // strict aliasing
    free(ptr);
  }

  _free(f);
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
    _future_fini,
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


/// Initialize a block of futures.
static int
_future_block_init_action(uint32_t *args) {
  hpx_addr_t target = hpx_thread_current_target();
  _newfuture_t *futures = NULL;

  // application level forwarding if the future block has moved
  if (!hpx_gas_try_pin(target, (void**)&futures))
    return HPX_RESEND;

  // sequentially initialize each future
  uint32_t size = args[0];
  uint32_t block_size = args[1];
  for (uint32_t i = 0; i < block_size; ++i)
    _future_init(&futures[i], size, (bool)args[3]);

  hpx_gas_unpin(target);
  return HPX_SUCCESS;
}


/// Initialize a strided block of futures
static int
_future_blocks_init_action(uint32_t *args) {
  hpx_addr_t      base = hpx_thread_current_target();
  uint32_t  block_size = args[1];
  uint32_t block_bytes = block_size * sizeof(_newfuture_t);
  uint32_t      blocks = args[2];

  hpx_addr_t and = hpx_lco_and_new(blocks);
  for (uint32_t i = 0; i < blocks; i++) {
    hpx_addr_t block = hpx_addr_add(base, i * here->ranks * block_bytes);
    hpx_call(block, _future_block_init, args, 2 * sizeof(*args), and);
  }
  hpx_lco_wait(and);
  hpx_lco_delete(and, HPX_NULL);

  return HPX_SUCCESS;
}

#if 0
static hpx_addr_t
_newfuture_new(size_t size, bool shared) {
  hpx_addr_t f;
  _newfuture_t *local = _free_futures;
  if (local) {
    _free_futures = (_newfuture_t*)local->value;
    f = HPX_HERE;
    char *base;
    if (!hpx_gas_try_pin(f, (void**)&base)) {
      dbg_error("future: could not translate local block.\n");
    }
    f.offset = (char*)local - base;
    assert(f.offset < f.block_bytes);
  }
  else {
    f = locality_malloc(sizeof(_newfuture_t));
    if (!hpx_gas_try_pin(f, (void**)&local)) {
      dbg_error("future: could not pin newly allocated future of size %llu.\n", (unsigned long long)size);
      hpx_abort();
    }
  }
  _future_init(local, size, shared);
  hpx_gas_unpin(f);
  return f;
}
#endif

hpx_newfuture_t *
hpx_lco_newfuture_new(size_t size) {
  return hpx_lco_newfuture_new_all(1, size);
  //return _newfuture_new(size, false);
  return NULL;
}

// assumes table is already locked
static void
_table_resize() {
  // TODO make nicer
  _newfuture_table.futs = realloc(_newfuture_table.futs, _newfuture_table.capacity * 2 * sizeof(hpx_newfuture_t*));
  _newfuture_table.capacity *= 2;
}

struct new_all_args {
  int n;
  int base_rank;
  size_t size;
};

static hpx_newfuture_t*
_new_all(struct new_all_args *args) {
	 
  int n = args->n; // number of futures
  int base_rank = args->base_rank;
  size_t size = args->size;

  size_t elem_size = size + sizeof(_newfuture_t);

  // allocate control structures that exist globally (via redundancy - they're not unique global objects)
  hpx_newfuture_t *base_local = calloc(n, sizeof(hpx_newfuture_t));

  // allocate local data
  int futs_here = n / hpx_get_num_ranks();
  futs_here = futs_here + (hpx_get_num_ranks() - futs_here % hpx_get_num_ranks());
  _newfuture_t *futures = calloc(futs_here, elem_size);

  // allocate local send buffer
  base_local[hpx_get_my_rank()].send_buffer = malloc(elem_size);

  void *send_buffer = base_local[hpx_get_my_rank()].send_buffer;

  sync_lockable_ptr_lock(&_newfuture_table.lock);
  _newfuture_table.index++;
  if (_newfuture_table.index > _newfuture_table.capacity)
    _table_resize();
  _newfuture_table.futs[_newfuture_table.index] = base_local;

  photon_register_buffer(send_buffer, elem_size);

  photon_register_buffer(futures, elem_size * futs_here);
  
  for (int i = 0; i < hpx_get_num_ranks(); i++) {
    if (i == hpx_get_my_rank())
      continue;
    photon_rid rid;
    photon_post_recv_buffer_rdma(i, futures, elem_size * futs_here, _NEWFUTURE_EXCHG, &rid);
    int dummy;
    photon_wait_any(&dummy, &rid); // make sure we actually do something
  }

  for (int i = 0; i < hpx_get_num_ranks(); i++) {
    base_local[i].count = n;
    base_local[i].base_rank = base_rank;
    base_local[i].size_per = size;
    base_local[i].id = i;
    base_local[i].send_buffer = send_buffer;
    base_local[i].table_index = _newfuture_table.index;

    if (i == hpx_get_my_rank())
      continue;
    photon_rid rid;
    // wait for a recv buffer that was posted
    photon_wait_recv_buffer_rdma(i, PHOTON_ANY_SIZE, _NEWFUTURE_EXCHG, &rid);
    photon_get_buffer_remote(rid, (struct photon_buffer_t*)&base_local[i].buffer);
  }

  sync_lockable_ptr_unlock(&_newfuture_table.lock);

  return base_local;

  // TODO return error on error
}

static int
_new_all_remote_action(struct new_all_args *args) {
  hpx_newfuture_t *fut = _new_all(args);
  if (fut != NULL)
    return HPX_SUCCESS;
  else
    return HPX_ERROR;
}

#if 0
static hpx_newfuture_t * 
_new_all(int n, size_t size, bool shared) {

  // perform the global allocation
  // if we want to be consistent with old futures, we'd need to have a 
  // parameter for block_size. Just in case, we keep block_size but set
  // it to 1.
  uint32_t block_size = 1;
  uint32_t blocks = (n / block_size) + ((n % block_size) ? 1 : 0);
  uint32_t block_bytes = block_size * sizeof(_newfuture_t);
  hpx_addr_t base = hpx_gas_global_alloc(blocks, block_bytes);

  // for each rank, send an initialization message
  uint32_t args[4] = {
    size,
    block_size,
    (blocks / here->ranks), // bks per rank
    (uint32_t)shared
  };

  // We want to do this in parallel, but wait for them all to complete---we
  // don't need any values from this broadcast, so we can use the and
  // reduction.
  int ranks = here->ranks;
  int rem = blocks % here->ranks;
  hpx_addr_t and[2] = {
    hpx_lco_and_new(ranks),
    hpx_lco_and_new(rem)
  };

  for (int i = 0; i < ranks; ++i) {
    hpx_addr_t there = hpx_addr_add(base, i * block_bytes);
    hpx_call(there, _future_blocks_init, args, sizeof(args), and[0]);
  }

  for (int i = 0; i < rem; ++i) {
    hpx_addr_t block = hpx_addr_add(base, args[2] * ranks + i * block_bytes);
    hpx_call(block, _future_block_init, args, 2 * sizeof(args[0]), and[1]);
  }

  hpx_lco_wait_all(2, and, NULL);
  hpx_lco_delete(and[0], HPX_NULL);
  hpx_lco_delete(and[1], HPX_NULL);

  // return the base address of the allocation
  return base;

}
#endif

hpx_newfuture_t *
hpx_lco_newfuture_new_all(int n, size_t size) {
  
  struct new_all_args args = {
    .n = n,
    .base_rank = hpx_get_my_rank(),
    .size = size
  };

  hpx_addr_t done = hpx_lco_and_new(hpx_get_num_ranks() - 1);
  hpx_newfuture_t *base_local = _new_all(&args);
  for (int i = 0; i < hpx_get_num_ranks(); i++) {
    if (i != hpx_get_my_rank())
      hpx_call(HPX_THERE(i), _new_all_remote, &args, sizeof(args), done);
  }
  hpx_lco_wait(done);
  
  return base_local;
}

hpx_newfuture_t *hpx_lco_newfuture_shared_new(size_t size) {
  // TODO
  //  return _newfuture_new(size, true);
  return NULL;
}

hpx_newfuture_t *hpx_lco_newfuture_shared_new_all(int num_participants, size_t size) {
  // TODO
  //  return _new_all(num_participants, size, true);
  return NULL;
}


// Application level programmer doesn't know how big the future is, so we
// provide this array indexer.
hpx_newfuture_t *
hpx_lco_newfuture_at(hpx_newfuture_t *array, int i) {
  return &array[i];
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


  struct photon_buffer_t *buffer = (struct photon_buffer_t *)&future->buffer;
  int remote_rank = _newfuture_get_rank(future);
  void *remote_ptr = (void*)_newfuture_get_data_addr(future);
  struct photon_buffer_priv_t remote_priv = buffer->priv;
  photon_rid local_rid = PHOTON_NOWAIT_TAG; // TODO if lsync != NULL use local_rid to represent local LCOs address
  photon_rid remote_rid = _newfuture_get_data_addr(future);
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

void hpx_lco_newfuture_setat(hpx_newfuture_t *future,  int id, size_t size, void *data,
				     hpx_addr_t lsync_lco, hpx_addr_t rsync_lco) {
  hpx_newfuture_t *future_i = hpx_lco_newfuture_at(future, id);

  // normally lco_set does all this
  if (_newfuture_get_rank(future_i) != hpx_get_my_rank()) {
    _put_with_completion(future_i, id, size, data, lsync_lco, rsync_lco);
  }
  else
    _future_set_with_copy((lco_t*)_newfuture_get_addr(future_i), size, data);  
}

void hpx_lco_newfuture_emptyat(hpx_newfuture_t *base, int i, hpx_addr_t rsync_lco) {
#if 0
  hpx_newfuture_t *target = hpx_lco_newfuture_at(base, i);
  
  // this is based on the implementation of hpx_lco_set()
  lco_t *lco = NULL;
  if (!hpx_gas_try_pin(target, (void**)&lco))
  {
    hpx_call_async(target, _future_reset_remote, NULL, 0, HPX_NULL, rsync_lco);
    return;
  }

  _future_reset(lco);
  hpx_gas_unpin(target);

  if (!hpx_addr_eq(rsync_lco, HPX_NULL))
    hpx_lco_set(rsync_lco, 0, NULL, HPX_NULL, HPX_NULL);
#endif
}

hpx_status_t hpx_lco_newfuture_getat(hpx_newfuture_t *base, int i, size_t size, void *value) {
  // TODO
  /*
  hpx_newfuture_t *target = hpx_lco_newfuture_at(base, i);
  return hpx_lco_get(target, size, value);
  */

  hpx_newfuture_t *future_i = hpx_lco_newfuture_at(base, i);

  if (_newfuture_get_rank(future_i) != hpx_get_my_rank()) {
    return HPX_ERROR;
    // TODO
  }

  lco_t *lco = (lco_t*)_newfuture_get_addr(future_i);
  return _future_get(lco, size, value);
}



// this is a highly suboptimal implementation
// ideally this would be done more like wait_all is implemented
void hpx_lco_newfuture_get_all(size_t num, hpx_newfuture_t *futures, size_t size,
			       void *values[]) {
  /*

    hpx_newfuture_t *lcos = malloc(sizeof(hpx_newfuture_t) * num);
    int *sizes = malloc(sizeof(int) * num);
    hpx_status_t *statuses = malloc(sizeof(hpx_status_t) * num);
    
    for (int i = 0; i < num; i++) {
    lcos[i] = hpx_lco_newfuture_at(futures, i);
    sizes[i] = size;
    }
    
    hpx_lco_get_all(num, lcos, sizes, values, statuses);
    
    free(lcos);
    free(sizes);
    free(statuses);
  */
  // TODO TODO TODO
}

void hpx_lco_newfuture_waitat(hpx_newfuture_t *future, int id, hpx_set_t set) {
  hpx_newfuture_t *future_i = hpx_lco_newfuture_at(future, id);

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
}

hpx_status_t hpx_lco_newfuture_waitat_for(hpx_newfuture_t *future, int id, hpx_set_t set, hpx_time_t time) {
  return HPX_ERROR;
}

hpx_status_t hpx_lco_newfuture_waitat_until(hpx_newfuture_t *future, int id, hpx_set_t set, hpx_time_t time) {
  return HPX_ERROR;
}

void hpx_lco_newfuture_wait_all(size_t num, hpx_newfuture_t *newfutures, hpx_set_t set) {
  hpx_addr_t done = hpx_lco_and_new(num);
  struct _future_wait_args *args = malloc(sizeof(args));
  args->set = set;
  args->wait_action = _future_wait_full; // TODO remove??

  for (int i = 0; i < num; i++) {
    hpx_newfuture_t *target = hpx_lco_newfuture_at(newfutures, i);
    hpx_call_async(HPX_THERE(_newfuture_get_rank(target)), _future_wait_remote, args, sizeof(args), HPX_NULL, done);
  }

  hpx_lco_wait(done);
  free(args);

  return;
}

hpx_status_t hpx_lco_newfuture_wait_all_for(size_t num, hpx_newfuture_t *newfutures, 
					    hpx_set_t set, hpx_time_t time) {
  return HPX_ERROR;
}

hpx_status_t hpx_lco_newfuture_wait_all_until(size_t num, hpx_newfuture_t *newfutures, 
					    hpx_set_t set, hpx_time_t time) {
  return HPX_ERROR;
}

void hpx_lco_newfuture_free(hpx_newfuture_t *future) {

  // TODO
  //  hpx_lco_delete(future, HPX_NULL);

}


void hpx_lco_newfuture_free_all(int num, hpx_newfuture_t *base) {
  // Ideally this would not need to know the number of futures. But in order to avoid that
  // we would need to add to futures that are created and deleted with _all() functions
  // a pointer to the base future and a count.

  // TODO
  //  for (int i = 0; i < num; i++)
  //    hpx_lco_delete(hpx_lco_newfuture_at(base, i), HPX_NULL);
}

bool hpx_lco_newfuture_is_shared(hpx_newfuture_t *target) {
  return false;
}

int
hpx_lco_newfuture_get_rank(hpx_newfuture_t *future) {
  return _newfuture_get_rank(future);
}

 

static void HPX_CONSTRUCTOR
_future_initialize_actions(void) {
  // these actions are for old futures - do we need them still?
  _future_block_init  = HPX_REGISTER_ACTION(_future_block_init_action);
  _future_blocks_init = HPX_REGISTER_ACTION(_future_blocks_init_action);

  // new
  _future_reset_remote = HPX_REGISTER_ACTION(_future_reset_remote_action);
  _future_wait_remote = HPX_REGISTER_ACTION(_future_wait_remote_action);

  _is_shared = HPX_REGISTER_ACTION(_is_shared_action);
  _future_set_no_copy_from_remote = HPX_REGISTER_ACTION(_future_set_no_copy_from_remote_action);

  _recv_queue_progress = HPX_REGISTER_ACTION(_recv_queue_progress_action);
  _send_queue_progress = HPX_REGISTER_ACTION(_send_queue_progress_action);
  _new_all_remote = HPX_REGISTER_ACTION(_new_all_remote_action);
}
