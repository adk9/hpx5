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
  volatile uint64_t state;
  void                 *base;
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


static uint64_t _write_lock(_record_t *record) {
  uint64_t state = sync_load_ui64(&record->state, SYNC_ACQUIRE);

  // spin while the line is locked
  if (_locked(state)) {
    scheduler_yield();
    return _write_lock(record);
  }

  // need the write lock for an update
  if (!_try_lock(record, state)) {
    scheduler_yield();
    return _write_lock(record);
  }

  // wait for the number of readers to drop to 0
  while (_readers(state)) {
    scheduler_yield();
    state = sync_load_ui64(&record->state, SYNC_ACQUIRE);
  }

  return state;
}

static void _unlock(_record_t *record, uint64_t state) {
  sync_store(&record->state, state & ~_LOCKED, SYNC_RELEASE);
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
  uint64_t state = sync_load_ui64(&record->state, SYNC_ACQUIRE);

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


/// Update the row to be a valid local mapping.
static void _agas_btt_insert(btt_class_t *btt, hpx_addr_t addr, void *base) {
  agas_btt_t *agas = (agas_btt_t *)btt;
  _record_t *record = &agas->table[addr_block_id(addr)];
  _write_lock(record);                          // don't care about state
  record->base = base;
  _unlock(record, _VALID + _LOCAL);
}


static bool _agas_btt_forward(btt_class_t *btt, hpx_addr_t addr, uint32_t rank,
                              void **out) {
  agas_btt_t *agas = (agas_btt_t *)btt;
  _record_t *record = &agas->table[addr_block_id(addr)];
  uint64_t state = _write_lock(record);

  // if the rank is us, don't do anything
  if (here->rank == rank) {
    _unlock(record, state);
    return false;
  }

  // if the user wants the base, give it to them, update the entry, and release
  // the write lock
  if (out)
    *out = record->base;
  record->base = (void*)(uintptr_t)rank;
  _unlock(record, _VALID);

  // if the old state was local, then the output value is the old base
  return (_local(state));
}


static uint32_t _agas_btt_home(btt_class_t *btt, hpx_addr_t addr) {
  return addr_block_id(addr) % here->ranks;
}


static uint32_t _agas_btt_owner(btt_class_t *btt, hpx_addr_t addr) {
  agas_btt_t *agas = (agas_btt_t *)btt;
  _record_t *record = &agas->table[addr_block_id(addr)];
  uint64_t state = sync_load_ui64(&record->state, SYNC_ACQUIRE);

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
    dbg_error("agas: could not allocate block-translation-table.\n");
    return NULL;
  }

  // set up class
  btt->class.delete  = _agas_btt_delete;
  btt->class.try_pin = _agas_btt_try_pin;
  btt->class.unpin   = _agas_btt_unpin;
  btt->class.forward = _agas_btt_forward;
  btt->class.insert  = _agas_btt_insert;
  btt->class.owner   = _agas_btt_owner;
  btt->class.home    = _agas_btt_home;

  // mmap the table
  int prot = PROT_READ | PROT_WRITE;
  int flags = MAP_ANON | MAP_PRIVATE | MAP_NORESERVE;
  btt->table = mmap(NULL, _TABLE_SIZE, prot, flags, -1, 0);
  if (btt->table == MAP_FAILED) {
    dbg_error("agas: could not mmap block translation table.\n");
    free(btt);
    return NULL;
  }

  return &btt->class;
}
