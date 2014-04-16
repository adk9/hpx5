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
  SYNC_ATOMIC(int64_t count);
  void * SYNC_ATOMIC() base;
} _record_t;

static const uint64_t _TABLE_SIZE = (uint64_t)UINT32_MAX * sizeof(_record_t);
#define _WRITE_LOCK 0X8000000000000000
#define _STATE 0x3
#define _READERS ~_WRITE_LOCK & ~_STATE

typedef struct {
  btt_class_t class;
  _record_t  *table;
} agas_btt_t;


static bool _invalid(int64_t count) {
  // least significant bit indicates a valid mapping
  return (count % 2 == 0);
}


static bool _forward(int64_t count) {
  // second least significant bit indicates a forward
  return ((count >> 1) % 2 != 0);
}


static bool _locked(int64_t count) {
  // most significant bit indicates lock
  return (count < 0);
}

static int64_t _readers(int64_t count) {
  return (count & _READERS);
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
  uint32_t block_id = addr_block_id(addr);
  int64_t count;
  sync_load(count, &agas->table[block_id].count, SYNC_ACQUIRE);

  if (_invalid(count))
    return false;

  if (_forward(count))
    return false;

  if (_locked(count))
    return false;

  // don't pin this if the client doesn't provide an output
  if (!out)
    return true;

  // if not ref count success, retry (could have changed arbitrarily)
  if (!sync_cas(&agas->table[block_id].count, count, count + 4, SYNC_RELEASE,
                SYNC_RELAXED))
    return _agas_btt_try_pin(btt, addr, out);

  void *base = agas->table[block_id].base;
  *out = addr_to_local(addr, base);
  return true;
}


static void _agas_btt_unpin(btt_class_t *btt, hpx_addr_t addr) {
  // assume that the programmer knows what they're talking about, just blindly
  // decrement by two
  agas_btt_t *agas = (agas_btt_t *)btt;
  uint32_t block_id = addr_block_id(addr);
  sync_fadd(&agas->table[block_id].count, -4, SYNC_ACQ_REL);
}


static void *_agas_btt_invalidate(btt_class_t *btt, hpx_addr_t addr) {
  agas_btt_t *agas = (agas_btt_t *)btt;
  uint32_t block_id = addr_block_id(addr);
  int64_t count;
  sync_load(count, &agas->table[block_id].count, SYNC_ACQUIRE);

  if (_invalid(count))
    return NULL;

  if (_locked(count))
    return NULL;

  // otherwise acquire the write lock
  if (!sync_cas(&agas->table[block_id].count, count, count | _WRITE_LOCK,
                SYNC_RELEASE, SYNC_RELAXED))
    return _agas_btt_invalidate(btt, addr);

  // wait for the readers to drain away---must use scheduler_yield for this
  while (_readers(count)) {
    scheduler_yield();
    sync_load(count, &agas->table[block_id].count, SYNC_ACQUIRE);
  }

  // clear the mapping
  void *base = agas->table[block_id].base;
  agas->table[block_id].base = NULL;

  // mark the row as invalid
  sync_store(&agas->table[block_id].count, 0, SYNC_RELEASE);

  // if the old mapping wasn't a forward, return it
  return (_forward(count)) ? NULL : base;
}


static void _agas_btt_insert(btt_class_t *btt, hpx_addr_t addr, void *base) {
  agas_btt_t *agas = (agas_btt_t *)btt;
  uint32_t block_id = addr_block_id(addr);

  if (DEBUG) {
    int64_t count;
    sync_load(count, &agas->table[block_id].count, SYNC_ACQUIRE);
    assert(_invalid(count));
  }

  // acquire the write lock
  if (!sync_cas(&agas->table[block_id].count, 0, _WRITE_LOCK, SYNC_RELEASE,
                SYNC_RELAXED)) {
    dbg_error("btt insert for existing mapping.\n");
    hpx_abort();
  }

  uint32_t blockid = addr_block_id(addr);
  uint64_t dst = block_id_macaddr(blockid);
  routing_t *routing = network_get_routing(here->network);

  routing_register_addr(routing, dst);
  // update the routing table
  int port = routing_my_port(routing);
  routing_add_flow(routing, HPX_SWADDR_WILDCARD, dst, port);

  agas->table[block_id].base = base;
  sync_store(&agas->table[block_id].count, 1, SYNC_RELEASE);
}


static uint32_t _agas_btt_home(btt_class_t *btt, hpx_addr_t addr) {
  return addr_block_id(addr) % here->ranks;
}


static uint32_t _agas_btt_owner(btt_class_t *btt, hpx_addr_t addr) {
  agas_btt_t *agas = (agas_btt_t *)btt;
  uint32_t block_id = addr_block_id(addr);
  int64_t count;
  sync_load(count, &agas->table[block_id].count, SYNC_ACQUIRE);

  // if I don't have a valid mapping for this, then assume it's at the home
  // locality
  if (_invalid(count))
    return _agas_btt_home(btt, addr);

  // if I have a cached forwarding address for this, then return it---doesn't
  // matter if this is wrong
  if (_forward(count))
    return (uint32_t)(uintptr_t)agas->table[block_id].base;

  // otherwise, it's mine
  return here->rank;
}


btt_class_t *btt_agas_switch_new(void) {
  // Allocate the object
  agas_btt_t *btt = malloc(sizeof(*btt));
  if (!btt) {
    dbg_error("could not allocate AGAS block-translation-table.\n");
    return NULL;
  }

  // set up class
  btt->class.delete     = _agas_btt_delete;
  btt->class.try_pin    = _agas_btt_try_pin;
  btt->class.unpin      = _agas_btt_unpin;
  btt->class.invalidate = _agas_btt_invalidate;
  btt->class.update     = NULL;
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
