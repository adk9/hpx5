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

/// ----------------------------------------------------------------------------
/// @brief The parcel layer.
///
/// Parcels are the foundation of HPX. The parcel structure serves as both the
/// actual, "on-the-wire," network data structure, as well as the
/// "thread-control-block" descriptor for the threading subsystem.
/// ----------------------------------------------------------------------------
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "hpx/hpx.h"

#include "libhpx/btt.h"
#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/network.h"
#include "libhpx/parcel.h"
#include "libhpx/scheduler.h"

static uintptr_t _INPLACE_MASK = 0x1;
static uintptr_t _OWNED_MASK   = 0x2;
static uintptr_t _STATE_MASK   = 0x3;

void hpx_parcel_set_action(hpx_parcel_t *p, const hpx_action_t action) {
  p->action = action;
}


void hpx_parcel_set_target(hpx_parcel_t *p, const hpx_addr_t addr) {
  p->target = addr;
}


void hpx_parcel_set_cont(hpx_parcel_t *p, const hpx_addr_t cont) {
  p->cont = cont;
}


void hpx_parcel_set_data(hpx_parcel_t *p, const void *data, int size) {
  if (size) {
    void *to = hpx_parcel_get_data(p);
    memcpy(to, data, size);
  }
}


hpx_action_t hpx_parcel_get_action(const hpx_parcel_t *p) {
  return p->action;
}


hpx_addr_t hpx_parcel_get_target(const hpx_parcel_t *p) {
  return p->target;
}


hpx_addr_t hpx_parcel_get_cont(const hpx_parcel_t *p) {
  return p->cont;
}


void *hpx_parcel_get_data(hpx_parcel_t *p) {
  return ((uintptr_t)p->ustack & _INPLACE_MASK) ? &p->data : (void*)p->data;
}


void
hpx_parcel_send(hpx_parcel_t *p, hpx_addr_t done) {
  hpx_parcel_send_sync(p);
  if (!hpx_addr_eq(done, HPX_NULL))
    hpx_lco_set(done, NULL, 0, HPX_NULL);
}


void
hpx_parcel_send_sync(hpx_parcel_t *p) {
  // check loopback via rank
  hpx_addr_t target = p->target;
  uint32_t owner = btt_owner(here->btt, target);
  if (owner == here->rank)
    scheduler_spawn(p);
  else
    network_tx_enqueue(here->network, p);
}


hpx_parcel_t *
hpx_parcel_acquire(void *data, size_t size) {
  // get a parcel of the right size from the allocator, the returned parcel
  // already has its data pointer and size set appropriately
  hpx_parcel_t *p = network_malloc(sizeof(*p) + size, HPX_CACHELINE_SIZE);
  if (!p) {
    dbg_error("failed to get an %lu-byte parcel from the allocator.\n", size);
    return NULL;
  }
  p->ustack = (struct ustack*)(_INPLACE_MASK | _OWNED_MASK);
  p->src    = here->rank;
  p->size   = size;
  p->action = HPX_ACTION_NULL;
  p->target = HPX_HERE;
  p->cont   = HPX_NULL;
  return p;
}


void
hpx_parcel_release(hpx_parcel_t *p) {
  network_free(p);
}

void
parcel_set_stack(hpx_parcel_t *p, struct ustack *stack) {
  assert((uintptr_t)stack % sizeof(void*) == 0);
  uintptr_t state = (uintptr_t)p->ustack & _STATE_MASK;
  p->ustack = (struct ustack*)(state | (uintptr_t)stack);
}

struct ustack *
parcel_get_stack(hpx_parcel_t *p) {
  return (struct ustack*)((uintptr_t)p->ustack & ~_STATE_MASK);
}
