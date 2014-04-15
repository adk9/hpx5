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
#include "addr.h"

typedef struct {
  SYNC_ATOMIC(void *base);
  SYNC_ATOMIC(uint64_t count);
} _record_t;

static const uint64_t _TABLE_SIZE = UINT32_MAX * sizeof(_record_t);

typedef struct {
  btt_class_t class;
  _record_t  *table;
} agas_btt_t;

static uint32_t _agas_btt_owner(btt_class_t *btt, hpx_addr_t addr) {
  return 0;
}


static uint32_t _agas_btt_home(btt_class_t *btt, hpx_addr_t addr) {
  return addr_block_id(addr) % here->ranks;
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
  return false;
}


static void _agas_btt_unpin(btt_class_t *btt, hpx_addr_t addr) {
  // noop for AGAS
}


static void _agas_btt_invalidate(btt_class_t *btt, hpx_addr_t addr) {
  // noop for AGAS
}


static void _agas_btt_insert(btt_class_t *btt, hpx_addr_t addr, void *base) {
}

static void _agas_btt_remap(btt_class_t *btt, hpx_addr_t src, hpx_addr_t dst,
                            hpx_addr_t lco) {
}


btt_class_t *btt_agas_new(void) {
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
  btt->class.insert     = _agas_btt_insert;
  btt->class.remap      = _agas_btt_remap;
  btt->class.owner      = _agas_btt_owner;
  btt->class.home       = _agas_btt_home;

  // mmap the table
  int prot = PROT_READ | PROT_WRITE;
  int flags = MAP_ANON | MAP_PRIVATE | MAP_NORESERVE | MAP_NONBLOCK;
  btt->table = mmap(NULL, _TABLE_SIZE, prot, flags, -1, 0);
  if (!btt->table) {
    dbg_error("could not mmap AGAS block stranslation table.\n");
    free(btt);
    return NULL;
  }

  return &btt->class;
}
