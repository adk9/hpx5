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
#include "libhpx/network.h"
#include "libhpx/routing.h"
#include "addr.h"

static const uint64_t _TABLE_SIZE = (uint64_t)UINT32_MAX * sizeof(void*);

typedef void* SYNC_ATOMIC() atomic_word_t;

typedef struct {
  btt_class_t    class;
  atomic_word_t *table;
} pgas_btt_t;


static uint32_t _row(hpx_addr_t addr) {
  return addr_block_id(addr) / here->ranks;
}


static uint32_t _pgas_btt_owner(btt_class_t *btt, hpx_addr_t addr) {
  uint32_t block_id = addr_block_id(addr);
  return (btt->type == HPX_GAS_PGAS_SWITCH) ? block_id : block_id % here->ranks;
}


static uint32_t _pgas_btt_home(btt_class_t *btt, hpx_addr_t addr) {
  return addr_block_id(addr) % here->ranks;
}


static void _pgas_btt_delete(btt_class_t *btt) {
  pgas_btt_t *pgas = (pgas_btt_t*)btt;

  if (!pgas)
    return;

  if (pgas->table)
    munmap((void*)pgas->table, _TABLE_SIZE);

  free(pgas);
}

static bool _pgas_btt_try_pin(btt_class_t *btt, hpx_addr_t addr, void **out) {
  pgas_btt_t *pgas = (pgas_btt_t*)btt;

  // If I'm not the home for this addr, then I don't have a mapping for it.
  if (_pgas_btt_home(btt, addr) != here->rank)
    return false;

  // Look up the mapping
  void *base;
  sync_load(base, &pgas->table[_row(addr)], SYNC_ACQUIRE);
  if (!base)
    return false;

  // Return the local address, if the user wanted it.
  if (out)
    *out = addr_to_local(addr, base);

  return true;
}


static void _pgas_btt_unpin(btt_class_t *btt, hpx_addr_t addr) {
  // noop for PGAS
}


static bool _pgas_btt_forward(btt_class_t *btt, hpx_addr_t addr, uint32_t rank,
                              void **out) {
  return false;
}


static void _pgas_btt_insert(btt_class_t *btt, hpx_addr_t addr, void *base) {
  pgas_btt_t *pgas = (pgas_btt_t*)btt;

  if (btt->type == HPX_GAS_PGAS_SWITCH) {
    uint32_t blockid = addr_block_id(addr);
    uint64_t bmc = block_id_ipv4mc(blockid);
    uint64_t dst = block_id_macaddr(blockid);
    routing_t *routing = network_get_routing(here->network);

    routing_register_addr(routing, bmc);
    // update the routing table
    int port = routing_my_port(routing);
    routing_add_flow(routing, HPX_SWADDR_WILDCARD, dst, port);
  }

  // Just find the row, and store the base pointer there.
  sync_store(&pgas->table[_row(addr)], base, SYNC_RELEASE);
}


btt_class_t *btt_pgas_new(void) {
  // Allocate the object
  pgas_btt_t *btt = malloc(sizeof(*btt));
  if (!btt) {
    dbg_error("could not allocate PGAS block-translation-table.\n");
    return NULL;
  }

  // set up class
  btt->class.delete  = _pgas_btt_delete;
  btt->class.try_pin = _pgas_btt_try_pin;
  btt->class.unpin   = _pgas_btt_unpin;
  btt->class.insert  = _pgas_btt_insert;
  btt->class.forward = _pgas_btt_forward;
  btt->class.owner   = _pgas_btt_owner;
  btt->class.home    = _pgas_btt_home;

  // mmap the table
  int prot = PROT_READ | PROT_WRITE;
  int flags = MAP_ANON | MAP_PRIVATE | MAP_NORESERVE;
  btt->table = mmap(NULL, _TABLE_SIZE, prot, flags, -1, 0);
  if (btt->table == MAP_FAILED) {
    dbg_error("could not mmap PGAS block-translation-table, "
              "defaulting to HPX_GAS_NOGLOBAL.\n");
    free(btt);
    return btt_new(HPX_GAS_NOGLOBAL);
  }

  return &btt->class;
}
