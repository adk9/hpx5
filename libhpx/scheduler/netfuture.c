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
#include "libsync/queues.h"
#include "lco.h"
#include "cvar.h"

#define dbg_printf0(...)
//#define dbg_printf0 printf
#define dbg_printf(...)
//#define dbg_printf printf
#define PHOTON_NOWAIT_TAG 0
#define FT_SHARED 1<<3

#define PWC_QUEUE_T two_lock_queue_t
#define PWC_QUEUE_NEW sync_two_lock_queue_new
#define PWC_QUEUE_FINI sync_two_lock_queue_fini
#define PWC_QUEUE_ENQUEUE sync_two_lock_queue_enqueue
#define PWC_QUEUE_DEQUEUE sync_two_lock_queue_dequeue

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
  char data[] HPX_ALIGNED(__BIGGEST_ALIGNMENT__);
} _netfuture_t;

/// This data is used to locate netfutures within the system. An array of
/// these, representing each netfuture array, is stored within the netfuture
/// table at each locality.
typedef struct {
  int table_index;
  size_t offset; // from base
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
  struct photon_buffer_t *buffers; // one for each rank
  void* base;
  hpx_addr_t base_gas;
  size_t mem_size;
  _fut_info_t *fut_infos;

} _netfuture_table_t;

typedef struct {
  int remote_rank;
  void *data;
  size_t size;
  void* remote_ptr;
  struct photon_buffer_priv_t remote_priv;
  photon_rid local_rid;
  photon_rid remote_rid;
} pwc_args_t;

static hpx_netfuture_config_t _netfuture_cfg = HPX_NETFUTURE_CONFIG_DEFAULTS;
static const int _NETFUTURE_EXCHG = -37;
static bool shutdown = false;
static uint32_t _outstanding_sends = 0;
static uint32_t _outstanding_send_limit = 0;

static hpx_action_t _future_set_no_copy_from_remote = 0;
static hpx_action_t _progress = 0;
static hpx_action_t _progress_recv = 0;
static hpx_action_t _add_future_to_table = 0;
static hpx_action_t _initialize_netfutures = 0;

static _netfuture_table_t _netfuture_table = {.inited = 0};
//static lockable_ptr_t pwc_lock;
static PWC_QUEUE_T *pwc_q;

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
  return f->index % hpx_get_num_ranks(); // TODO change if we want to allow base_rank != 0
}

/// Returns a count of how many netfutures in a netfuture array are at each locality
/// (which is uniform across all ranks)
static int
_netfutures_at_rank(hpx_netfuture_t *f) {
  return (f->count + hpx_get_num_ranks() - 1)/hpx_get_num_ranks();
}

/// Return the offset for this netfuture (not the netfuture array) from the
/// base of all netfuture storage at each locality (the offset is the same
/// at all localitites).
/// @p f must have the proper netfuture index set.
static size_t
_netfuture_get_offset(hpx_netfuture_t *f) {
  size_t size = sizeof(_netfuture_t) + f->size;
  return _netfuture_table.fut_infos[f->table_index].offset + (size * (f->index / hpx_get_num_ranks()));
}

/// Return the native address of the _netfuture_t representation of a future
/// Returns the address of the netfuture itself, not the data it contains.
/// @p f must have the proper netfuture index set.
static uintptr_t
_netfuture_get_addr(hpx_netfuture_t *f) {
  uintptr_t offset =  _netfuture_get_offset(f);
  int rank = _netfuture_get_rank(f);
  uintptr_t rank_base = _netfuture_table.buffers[rank].addr;
  return rank_base + offset;
}

/// Return the native address of the _netfuture_t representation of the
/// future's data.
/// Returns the address of the netfuture's data only, not the netfuture
/// itself.
/// @p f must have the proper netfuture index set.
static uintptr_t
_netfuture_get_data_addr(hpx_netfuture_t *f) {
  return _netfuture_get_addr(f) + sizeof(_netfuture_t);
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
  dbg_printf("  _future_set_no_copy_from_remote_action on %p\n", (void*)f);
  lco_lock(&f->lco);
  if (!_empty(f))
    scheduler_wait(&f->lco.lock, &f->empty);
  _future_set_no_copy(f);
  lco_unlock(&f->lco);
  dbg_printf("  _future_set_no_copy_from_remote_action on %p DONE\n", (void*)f);
  return HPX_SUCCESS;
}

static void
_progress_sends() {
  int phstat;
  if (_outstanding_sends < _outstanding_send_limit) {
    pwc_args_t *pwc_args = PWC_QUEUE_DEQUEUE(pwc_q);
    if (pwc_args != NULL) {
      _outstanding_sends++;
      dbg_printf0("Progress thread putting to %p on %d from %d\n", pwc_args->remote_ptr, pwc_args->remote_rank, hpx_get_my_rank());
      assert((size_t)pwc_args->remote_ptr + pwc_args->size <
             _netfuture_table.buffers[pwc_args->remote_rank].addr +
             _netfuture_cfg.max_size);
      do {
    phstat =
      photon_put_with_completion(pwc_args->remote_rank,
                     pwc_args->data, pwc_args->size,
                     pwc_args->remote_ptr, pwc_args->remote_priv,
                     pwc_args->local_rid, pwc_args->remote_rid,
                     0);
    assert(phstat != PHOTON_ERROR);
      } while (phstat == PHOTON_ERROR_RESOURCE);
      free(pwc_args);
    }
  }
}

static void
_progress_send_completions() {
  int phstat;
  int flag;
  photon_rid request;
  phstat = photon_probe_completion(PHOTON_ANY_SOURCE, &flag, &request, PHOTON_PROBE_EVQ);
  //assert(phstat == PHOTON_OK);
  //    printf("photon_probe_completion = %d\n", phstat);
  if (phstat != PHOTON_OK)
    dbg_printf0("Event %d on %d for request %"PRIx64" with flag %d in probe(PHOTON_PROBE_EVQ)\n", phstat, hpx_get_my_rank(), request, flag);
  if (flag > 0) {
    dbg_printf("  Received send completion on rank %d for %" PRIx64 "\n", hpx_get_my_rank(), request);
    //      sync_fadd(&_outstanding_sends, -1, SYNC_RELEASE);
    _outstanding_sends--;
    if (request != 0){
      lco_t *lco = (lco_t*)request;
      lco_future_set(lco, 0, NULL);
    }
  }
}

static void
_progress_recvs() {
  int phstat;
  int flag;
  photon_rid request;
  phstat = photon_probe_completion(PHOTON_ANY_SOURCE, &flag, &request, PHOTON_PROBE_LEDGER);
  assert(phstat == PHOTON_OK);
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
}

static void
_progress_recv_action() {
  while (!shutdown) {
    _progress_recvs();
    hpx_thread_yield();
  }
}

static void
_progress_body() {
  if (_netfuture_table.inited != 1)
    return;
  _progress_sends();
  _progress_send_completions();
  //_progress_recvs();
}

/// This action handles all Photon completions, local and remote, affecting
/// the netfutures system.
static int
_progress_action(void *args) {
  //  int send_rank = -1;
  //  int i = 0;
  while (!shutdown) {
    _progress_body();
    /*
    i = (i + 1) % YIELD_COUNT;
    if (i == 0)
      hpx_thread_yield();
    */
    hpx_thread_yield();
  }
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
  hpx_addr_t ag;
  hpx_netfuture_config_t cfg;
} _nf_init_args_t;

/// Initialize the netfutures system at this locality.
static int
_initialize_netfutures_action(_nf_init_args_t *args) {
  hpx_addr_t ag = args->ag;
  if (hpx_get_my_rank())
    _netfuture_cfg = args->cfg;

  dbg_printf("  Initializing futures on rank %d\n", hpx_get_my_rank());

  _outstanding_send_limit = here->transport->get_send_limit(here->transport);
  //_outstanding_send_limit = 1;
  //pwc_lock = malloc(sizeof(*pwc_lock) * HPX_LOCALITIES);
  //assert(pwc_lock);

  dbg_printf("  Initializing futures on rank %d\n", hpx_get_my_rank());

  pwc_q = PWC_QUEUE_NEW();

  _table_lock();
  _netfuture_table.curr_index = 0;
  _netfuture_table.curr_capacity = _netfuture_cfg.max_array_number;
  _netfuture_table.curr_offset = 0;
  _netfuture_table.buffers = calloc(hpx_get_num_ranks(), sizeof(struct photon_buffer_t));
  _netfuture_table.fut_infos = calloc(_netfuture_table.curr_capacity, sizeof(_fut_info_t)) ;
  _netfuture_table.mem_size = _netfuture_cfg.max_size;
  _netfuture_table.base_gas = hpx_gas_alloc(_netfuture_cfg.max_size);
  assert(_netfuture_table.base_gas != HPX_NULL);
  dbg_printf("GAS base = 0x%"PRIx64".\n", _netfuture_table.base_gas);
  assert(hpx_gas_try_pin(_netfuture_table.base_gas, &_netfuture_table.base));

#if DEBUG
  memset(_netfuture_table.base, -1, _netfuture_cfg.max_size);
#endif

  dbg_printf("  At %d netfutures base = %p\n", hpx_get_my_rank(), _netfuture_table.base);
  dbg_printf0("  At %d netfutures base = %p top = %p\n", hpx_get_my_rank(), _netfuture_table.base, _netfuture_table.base + _netfuture_cfg.max_size);

  if (hpx_get_num_ranks() > 1) {
    for (int i = 0; i < hpx_get_num_ranks(); i++) {
      transport_t *transport = here->transport;
      memcpy(&_netfuture_table.buffers[i].priv, &transport->rkey_table[i].rkey, sizeof(_netfuture_table.buffers[i].priv));
      dbg_printf("  At rank %d, buffer[%d].priv = %"PRIx64",%"PRIx64"\n", hpx_get_my_rank(), i, _netfuture_table.buffers[i].priv.key0,  _netfuture_table.buffers[i].priv.key1);
    }
  }
  struct photon_buffer_t *buffer = &_netfuture_table.buffers[hpx_get_my_rank()];
  buffer->addr = (uintptr_t)_netfuture_table.base;
  hpx_lco_allgather_setid(ag, hpx_get_my_rank(),
              sizeof(struct photon_buffer_t), buffer,
              HPX_NULL, HPX_NULL);
  hpx_lco_get(ag, hpx_get_num_ranks() * sizeof(struct photon_buffer_t), _netfuture_table.buffers);
  // Note that we don't really need the whole buffers, just the buffer[i].addr...

  _netfuture_table.inited = 1;

  hpx_call_async(HPX_HERE, _progress, NULL, 0, HPX_NULL, HPX_NULL);
  hpx_call_async(HPX_HERE, _progress_recv, NULL, 0, HPX_NULL, HPX_NULL);

  _table_unlock();

  dbg_printf("  Initialized futures on rank %d\n", hpx_get_my_rank());
  return HPX_SUCCESS;
}

void hpx_netfutures_fini() {
  shutdown = true;
}

hpx_status_t hpx_netfutures_init(hpx_netfuture_config_t *cfg) {
  if (cfg != NULL) {
    assert(cfg->max_size != 0);
    assert(cfg->max_array_number != 0);
    _netfuture_cfg.max_size = cfg->max_size + cfg->max_number*sizeof(_netfuture_t);
    _netfuture_cfg.max_number = cfg->max_number;
    _netfuture_cfg.max_array_number = cfg->max_array_number;
  }

  // gas_alloc() only let's us use 32 bits for requesting memory
  assert(_netfuture_cfg.max_size < UINT32_MAX);

  printf("Initializing netfutures with %zu bytes per rank\n", _netfuture_cfg.max_size);

  hpx_addr_t ag = hpx_lco_allgather_new(hpx_get_num_ranks(), sizeof(struct photon_buffer_t));
  if (hpx_get_my_rank() != 0)
    return HPX_ERROR;
  hpx_addr_t done = hpx_lco_and_new(hpx_get_num_ranks());
  _nf_init_args_t init_args = {.ag = ag, .cfg = _netfuture_cfg};
  for (int i = 0; i < hpx_get_num_ranks(); i++)
    hpx_call(HPX_THERE(i), _initialize_netfutures, &init_args, sizeof(init_args), done);
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

  _netfuture_table.curr_offset += (_netfutures_at_rank(f)) * (f->size + sizeof(_netfuture_t));
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
    dbg_printf0("  Initing future on rank %d at %p\n", hpx_get_my_rank(), (void*)nf);
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

  int num_futures_per_rank = (n / hpx_get_num_ranks()) + ((n % hpx_get_num_ranks()) % 2);
  assert(_netfuture_table.curr_offset + num_futures_per_rank * (size + sizeof(_netfuture_t))
     <= _netfuture_cfg.max_size);

    assert(_netfuture_table.curr_index < _netfuture_cfg.max_array_number);



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
  hpx_netfuture_t fut;
  return fut;
}

hpx_netfuture_t hpx_lco_netfuture_shared_new_all(int num_participants, size_t size) {
  hpx_netfuture_t fut;
  return fut;
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

static void
_enqueue_put_with_completion(hpx_netfuture_t *future,  int id, size_t size, void *data,
              hpx_addr_t lsync_lco, hpx_addr_t rsync_lco) {

  // need the following information:

  // need to convey the following information:
  // local_rid to whomever is waiting on it locally
  // remote_rid to same

  // remote_rid can make the same as the identifier for the future being set
  // local_rid can be the same?

  pwc_args_t *args = malloc(sizeof(*args));
  hpx_netfuture_t *f = future;

  args->remote_rank = _netfuture_get_rank(future);
  assert(args->remote_rank == id % hpx_get_num_ranks());

  args->remote_ptr = (void*)_netfuture_get_data_addr(future);

  struct photon_buffer_t *buffer = &_netfuture_table.buffers[_netfuture_get_rank(f)];
  args->remote_priv = buffer->priv;

  if (lsync_lco == HPX_NULL)
    args->local_rid = PHOTON_NOWAIT_TAG; // TODO if lsync != NULL use local_rid to represent local LCOs address
  else {
    void *ptr;
    hpx_gas_try_pin(lsync_lco, &ptr);
    args->local_rid = (photon_rid)ptr;
  }
  args->remote_rid = _netfuture_get_addr(future);
  dbg_printf("  pwc with at %d for %d remote_rid == %" PRIx64 "\n", hpx_get_my_rank(), args->remote_rank, args->remote_rid);

  args->data = data;
  args->size = size;

  if ((size_t)args->remote_ptr + size >=
      _netfuture_table.buffers[args->remote_rank].addr +
      _netfuture_cfg.max_size)
    printf("ERROR on %d: bad address: %zu >= %zu on %d\n",
           hpx_get_my_rank(),
           (uintptr_t)((char*)args->remote_ptr + size),
           _netfuture_table.buffers[args->remote_rank].addr + _netfuture_cfg.max_size,
           args->remote_rank);

  assert((size_t)args->remote_ptr <
     _netfuture_table.buffers[args->remote_rank].addr +
     _netfuture_cfg.max_size);

  PWC_QUEUE_ENQUEUE(pwc_q, args);
}

void hpx_lco_netfuture_setat(hpx_netfuture_t future, int id, size_t size, hpx_addr_t value,
                 hpx_addr_t lsync_lco, hpx_addr_t rsync_lco) {

  //  if (!(id >= 0 && id <= future.count))
  //    printf("Error id = %d is not in valid range >= 0 <= %d !!!\n", id, future.count);
  assert(id >= 0 && id <= future.count);
  void *data;
  assert(hpx_gas_try_pin(value, &data)); // TODO: unpin this

  hpx_netfuture_t future_i = hpx_lco_netfuture_at(future, id);

  dbg_printf("  Setating to %d (%d, future at %p) from %d\n", future_i.index, _netfuture_get_rank(&future_i), (void*)_netfuture_get_addr(&future_i), hpx_get_my_rank());
  void *lco_addr;
  hpx_gas_try_pin(lsync_lco, &lco_addr);
  dbg_printf0("  Setating to %d (%d, future at %p data at %p) with lco %p from %d\n", future_i.index, _netfuture_get_rank(&future_i), (void*)_netfuture_get_addr(&future_i), (void*)_netfuture_get_data_addr(&future_i), lco_addr, hpx_get_my_rank());
  hpx_gas_unpin(lsync_lco);

  // normally lco_set does all this
  if (_netfuture_get_rank(&future_i) != hpx_get_my_rank()) {
    dbg_printf0("  Enqueuing setat to %d (%d, future at %p) from %d\n", future_i.index, _netfuture_get_rank(&future_i), (void*)_netfuture_get_addr(&future_i), hpx_get_my_rank());
    _enqueue_put_with_completion(&future_i, id, size, data, lsync_lco, rsync_lco);
  }
  else {
    _future_set_with_copy((_netfuture_t*)_netfuture_get_addr(&future_i), size, data);
    if (!(lsync_lco == HPX_NULL))
      hpx_lco_set(lsync_lco, 0, NULL, HPX_NULL, HPX_NULL);
    if (!(rsync_lco == HPX_NULL))
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

  _future_signal_empty(f);
  lco_unlock(&f->lco);
}

hpx_addr_t hpx_lco_netfuture_getat(hpx_netfuture_t base, int i, size_t size) {
  assert(i >= 0 && i <= base.count);

  hpx_netfuture_t future_i = hpx_lco_netfuture_at(base, i);
  assert(_netfuture_get_rank(&future_i) == hpx_get_my_rank());

  hpx_addr_t retval = HPX_NULL;
  lco_t *lco = (lco_t*)_netfuture_get_addr(&future_i);

  dbg_printf("  Getating %d from (%d, future at %p) to %d\n", future_i.index, _netfuture_get_rank(&future_i), (void*)_netfuture_get_addr(&future_i), hpx_get_my_rank());

  retval = hpx_addr_add(_netfuture_table.base_gas, _netfuture_get_offset(&future_i) + sizeof(_netfuture_t), 1);
  _future_get(lco, size, NULL, false);

  dbg_printf("  Done getting from (%d, %p) to %d\n", _netfuture_get_rank(&future_i), (void*)_netfuture_get_addr(&future_i), hpx_get_my_rank());
  return retval;
}

void hpx_lco_netfuture_get_all(size_t num, hpx_netfuture_t futures, size_t size,
                   void *values[]) {
}

void hpx_lco_netfuture_waitat(hpx_netfuture_t future, int id, hpx_set_t set) {
}

hpx_status_t hpx_lco_netfuture_waitat_for(hpx_netfuture_t future, int id, hpx_set_t set, hpx_time_t time) {
  return HPX_ERROR;
}

hpx_status_t hpx_lco_netfuture_waitat_until(hpx_netfuture_t future, int id, hpx_set_t set, hpx_time_t time) {
  return HPX_ERROR;
}

void hpx_lco_netfuture_wait_all(size_t num, hpx_netfuture_t netfutures, hpx_set_t set) {
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
  LIBHPX_REGISTER_ACTION(&_future_set_no_copy_from_remote, _future_set_no_copy_from_remote_action);
  LIBHPX_REGISTER_ACTION(&_progress, _progress_action);
  LIBHPX_REGISTER_ACTION(&_progress_recv, _progress_recv_action);
  LIBHPX_REGISTER_ACTION(&_add_future_to_table, _add_future_to_table_action);
  LIBHPX_REGISTER_ACTION(&_initialize_netfutures, _initialize_netfutures_action);
}

void (*netfuture_progress)() = _progress_body;
