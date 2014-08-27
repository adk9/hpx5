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

typedef void* SYNC_ATOMIC() atomic_word_t;

typedef struct {
  btt_class_t    class;
  atomic_word_t  local;
} _btt_t;


static uint32_t _home(btt_class_t *btt, hpx_addr_t addr) {
  return addr_block_id(addr) % here->ranks;
}


static uint32_t _owner(btt_class_t *btt, hpx_addr_t addr) {
  return _home(btt, addr);
}


static void _delete(btt_class_t *btt) {
  if (!btt)
    return;

  free(btt);
}


static bool _try_pin(btt_class_t *class, hpx_addr_t addr, void **out) {
  _btt_t *btt = (_btt_t*)class;

  // If I'm not the home for this addr, then I don't have a mapping for it.
  if (_home(class, addr) != here->rank)
    return false;

  // Return the local address, if the user wanted it.
  if (out)
    *out = addr_to_local(addr, btt->local);

  return true;
}


static void _unpin(btt_class_t *btt, hpx_addr_t addr) {
  // noop for PGAS
}


static bool _forward(btt_class_t *btt, hpx_addr_t addr, uint32_t rank,
                     void **out) {
  return false;
}


static void _insert(btt_class_t *class, hpx_addr_t addr, void *base) {
  _btt_t *btt = (_btt_t*)class;

  // Just find the row, and store the base pointer there.
  assert(_home(class, addr) == here->rank);
  sync_store(&btt->local, base, SYNC_RELEASE);
}


btt_class_t *btt_local_only_new(void) {
  // Allocate the object
  _btt_t *btt = malloc(sizeof(*btt));
  if (!btt) {
    dbg_error("local: could not allocate block-translation-table.\n");
    return NULL;
  }

  // set up class
  btt->class.delete  = _delete;
  btt->class.try_pin = _try_pin;
  btt->class.unpin   = _unpin;
  btt->class.insert  = _insert;
  btt->class.forward = _forward;
  btt->class.owner   = _owner;
  btt->class.home    = _home;
  btt->local         = NULL;
  return &btt->class;
}
