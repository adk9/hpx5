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

#include <stdlib.h>
#include <sys/mman.h>
#include "libsync/sync.h"
#include "libhpx/btt.h"
#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/scheduler.h"
#include "libhpx/network.h"
#include "libhpx/routing.h"
#include "addr.h"

typedef struct {
  SYNC_ATOMIC(uint64_t state);
  void *base;
} _record_t;


static const uint64_t _VALID    = 1;
static const uint64_t _LOCAL    = 2;
static const uint64_t _LOCKED   = 4;
static const uint64_t _STATE    = 7;
static const uint64_t _COUNT    = 8;
static const uint64_t _REFCOUNT = (UINT64_MAX - 7);

static bool _try_lock(_record_t *record, uint64_t state) {
  return sync_cas(&record->state, state, state | _LOCKED, SYNC_ACQUIRE,
                  SYNC_RELAXED);
}

typedef struct {
  btt_class_t class;
  _record_t  *table;
} agas_btt_t;


static const uint64_t _TABLE_SIZE = (uint64_t)UINT32_MAX * sizeof(_record_t);


static bool _valid(uint64_t state) {
  return (state & _VALID) == _VALID;
}


static bool _local(uint64_t state) {
  int64_t mask = _LOCAL | _VALID;
  return (state & mask) == mask;
}


static bool _locked(uint64_t state) {
  return (state & _LOCKED) == _LOCKED;
}


static uint64_t _readers(uint64_t state) {
  return (state & _REFCOUNT);
}


static void _agas_btt_delete(btt_class_t *btt) {
  agas_btt_t *agas = (agas_btt_t*)btt;

  if (!agas)
    return;

  if (agas->table)
    munmap((void*)agas->table, _TABLE_SIZE);

  free(agas);
}


static bool _agas_btt_try_pin(btt_class_t *btt, hpx_addr_t addr, void **out) {
  agas_btt_t *agas = (agas_btt_t *)btt;
  _record_t *record = &agas->table[addr_block_id(addr)];
  uint64_t state;
  sync_load(state, &record->state, SYNC_ACQUIRE);

  if (!_valid(state))
    return false;

  if (!_local(state))
    return false;

  if (_locked(state))
    return false;

  // don't pin this if the client doesn't provide an output
  if (!out)
    return true;

  // load the base address now, the refcount bump below will assure that we've
  // read this consistently with the state
  void *base = record->base;

  // if not ref count success, retry (could have changed arbitrarily)
  if (!sync_cas(&record->state, state, state + _COUNT, SYNC_RELEASE,
                SYNC_RELAXED))
    return _agas_btt_try_pin(btt, addr, out);

  // if the client wanted the base, then output it
  *out = addr_to_local(addr, base);
  return true;
}


static void _agas_btt_unpin(btt_class_t *btt, hpx_addr_t addr) {
  // assume that the programmer knows what they're talking about, just blindly
  // decrement by the amount we need to reduce a reference count
  agas_btt_t *agas = (agas_btt_t *)btt;
  _record_t *record = &agas->table[addr_block_id(addr)];
  sync_fadd(&record->state, - _COUNT, SYNC_ACQ_REL);
}


static void *_agas_btt_update(btt_class_t *btt, hpx_addr_t addr, uint32_t rank)
{
  assert(rank < here->ranks);
  agas_btt_t *agas = (agas_btt_t *)btt;
  _record_t *record = &agas->table[addr_block_id(addr)];
  uint64_t state;
  sync_load(state, &record->state, SYNC_ACQUIRE);

  // should we wait?
  if (_locked(state))
    return NULL;

  // need the write lock for an update
  if (!_try_lock(record, state))
    return _agas_btt_update(btt, addr, rank);

  // wait for the number of readers to drop to 0
  while (_readers(state)) {
    scheduler_yield();
    sync_load(state, &record->state, SYNC_ACQUIRE);
  }

  // if the line is local, then we'll want to return the current mapping
  void *local = NULL;
  if (_local(state))
    local = record->base;

  record->base = (void*)(uintptr_t)rank;

  // and update the line's state
  sync_store(&record->state, _VALID, SYNC_RELEASE);
  return local;
}


static void *_agas_btt_invalidate(btt_class_t *btt, hpx_addr_t addr) {
  agas_btt_t *agas = (agas_btt_t *)btt;
  _record_t *record = &agas->table[addr_block_id(addr)];
  int64_t state;
  sync_load(state, &record->state, SYNC_ACQUIRE);

  if (!_valid(state))
    return NULL;

  // should we wait?
  if (_locked(state))
    return NULL;

  // otherwise acquire the write lock
  if (!_try_lock(record, state))
    return _agas_btt_invalidate(btt, addr);

  // wait for the readers to drain away---must use scheduler_yield for this
  while (_readers(state)) {
    scheduler_yield();
    sync_load(state, &record->state, SYNC_ACQUIRE);
  }

  // if the line is local, we want to return the mapping
  void *local = NULL;
  if (_local(state))
    local = record->base;

  // clear the mapping
  record->base = NULL;

  // mark the row as invalid
  sync_store(&record->state, 0, SYNC_RELEASE);

  // if the old mapping wasn't a forward, return it
  return local;
}


static void _agas_btt_insert(btt_class_t *btt, hpx_addr_t addr, void *base) {
  agas_btt_t *agas = (agas_btt_t *)btt;
  _record_t *record = &agas->table[addr_block_id(addr)];
  int64_t state;
  sync_load(state, &record->state, SYNC_ACQUIRE);

  // should we wait?
  if (_locked(state))
    return;

  if (!_try_lock(record, state)) {
    _agas_btt_insert(btt, addr, base);
    return;
  }

  assert(!_valid(state) || !_local(state));

  record->base = base;
  sync_store(&record->state, _VALID | _LOCAL, SYNC_RELEASE);
}


static uint32_t _agas_btt_home(btt_class_t *btt, hpx_addr_t addr) {
  return addr_block_id(addr) % here->ranks;
}


static uint32_t _agas_btt_owner(btt_class_t *btt, hpx_addr_t addr) {
  agas_btt_t *agas = (agas_btt_t *)btt;
  _record_t *record = &agas->table[addr_block_id(addr)];
  int64_t state;
  sync_load(state, &record->state, SYNC_ACQUIRE);

  // if I don't have a valid mapping for this, then assume it's at the home
  // locality
  if (!_valid(state))
    return _agas_btt_home(btt, addr);

  // If it's local, then it's mine.
  if (_local(state))
    return here->rank;

  // otherwise, I have a forwarding address
  return (uint32_t)(uintptr_t)record->base;
}


btt_class_t *btt_agas_new(void) {
  // Allocate the object
  agas_btt_t *btt = malloc(sizeof(*btt));
  if (!btt) {
    dbg_error("could not allocate AGAS block-translation-table.\n");
    return NULL;
  }

  // set up class
  btt->class.type       = HPX_GAS_AGAS;
  btt->class.delete     = _agas_btt_delete;
  btt->class.try_pin    = _agas_btt_try_pin;
  btt->class.unpin      = _agas_btt_unpin;
  btt->class.invalidate = _agas_btt_invalidate;
  btt->class.update     = _agas_btt_update;
  btt->class.insert     = _agas_btt_insert;
  btt->class.owner      = _agas_btt_owner;
  btt->class.home       = _agas_btt_home;

  // mmap the table
  int prot = PROT_READ | PROT_WRITE;
  int flags = MAP_ANON | MAP_PRIVATE | MAP_NORESERVE | MAP_NONBLOCK;
  btt->table = mmap(NULL, _TABLE_SIZE, prot, flags, -1, 0);
  if (btt->table == MAP_FAILED) {
    dbg_error("could not mmap AGAS block stranslation table.\n");
    free(btt);
    return NULL;
  }

  return &btt->class;
}
