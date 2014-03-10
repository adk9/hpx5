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
#include "hpx.h"
#include "parcel.h"
#include "allocator.h"
#include "debug.h"

#define _INPLACE_BIT 0x1
#define _STATE_MASK 0x7
#define _DATA_MASK ~_STATE_MASK

/// sets the data pointer, without disturbing the packed state
void
parcel_set_data(hpx_parcel_t *p, void *n) {
  uintptr_t state = (uintptr_t)p->data;
  state &= _STATE_MASK;
  uintptr_t next = (uintptr_t)n;
  next |= state;
  p->data = (void*)next;
}


/// sets the data pointer to point at a parcel, used for freelisting
void
parcel_set_next(hpx_parcel_t *p, hpx_parcel_t *n) {
  parcel_set_data(p, n);
}


/// get the data pointer as a parcel pointer, used for freelisting
hpx_parcel_t *
parcel_get_next(hpx_parcel_t *p) {
  return (hpx_parcel_t*)hpx_parcel_get_data(p);
}


/// checks the inplace state
int
parcel_is_inplace(const hpx_parcel_t *p) {
  uintptr_t state = (uintptr_t)p->data;
  return state & _INPLACE_BIT;
}


/// sets the inplace state
void
parcel_set_inplace(hpx_parcel_t *p) {
  uintptr_t data = (uintptr_t)p->data;
  data |= _INPLACE_BIT;
  p->data = (void*)data;
}


void
hpx_parcel_set_action(hpx_parcel_t *p, const hpx_action_t action) {
  p->action = action;
}


void
hpx_parcel_set_target(hpx_parcel_t *p, const hpx_addr_t addr) {
  p->target = addr;
}


void
hpx_parcel_set_cont(hpx_parcel_t *p, const hpx_addr_t cont) {
  p->cont = cont;
}


void
hpx_parcel_set_data(hpx_parcel_t *p, const void *data, int size) {
  if (size) {
    void *to = hpx_parcel_get_data(p);
    memcpy(to, data, size);
  }
}


hpx_action_t
hpx_parcel_get_action(const hpx_parcel_t *p) {
  return p->action;
}


hpx_addr_t
hpx_parcel_get_target(const hpx_parcel_t *p) {
  return p->target;
}


hpx_addr_t
hpx_parcel_get_cont(const hpx_parcel_t *p) {
  return p->cont;
}


void *
hpx_parcel_get_data(hpx_parcel_t *p) {
  uintptr_t state = (uintptr_t)p->data;
  state &= _DATA_MASK;
  return (void*)state;
}


void
parcel_init(hpx_parcel_t *p, int size) {
  p->size = size;
  if (parcel_is_inplace(p))
    parcel_set_data(p, &p->payload);
  else
    parcel_set_data(p, malloc(size));
}


/// clean up a parcel before it goes back into the cache
void
parcel_fini(hpx_parcel_t *p) {
  if (!parcel_is_inplace(p))
    free(hpx_parcel_get_data(p));
}


/// pop a single parcel off of the front of a parcel list
hpx_parcel_t *
parcel_pop(hpx_parcel_t **parcel) {
  hpx_parcel_t *p = *parcel;
  if (!p)
    return NULL;

  *parcel = parcel_get_next(p);
  return p;
}


/// just push the parcel onto the front of the list.
void
parcel_push(hpx_parcel_t **list, hpx_parcel_t *p) {
  parcel_set_next(p, *list);
  *list = p;
}


/// concatenate two parcel lists
void
parcel_cat(hpx_parcel_t **list, hpx_parcel_t *p) {
  // find the end of p
  hpx_parcel_t *end = p;
  while (parcel_get_next(end))
    end = parcel_get_next(end);

  // update the two pointers
  parcel_set_next(end, *list);
  *list = p;
}


hpx_parcel_t *
hpx_parcel_acquire(size_t size) {
  // get a parcel of the right size from the allocator, the returned parcel
  // already has its data pointer and size set appropriately
  hpx_parcel_t *p = parcel_allocator_get(size);
  if (!p) {
    dbg_error("failed to get an %lu-byte parcel from the allocator.\n", size);
    return NULL;
  }

  p->action = HPX_ACTION_NULL;
  p->target = HPX_HERE;
  p->cont   = HPX_NULL;
  return p;
}


void
hpx_parcel_release(hpx_parcel_t *p) {
  parcel_allocator_put(p);
}
