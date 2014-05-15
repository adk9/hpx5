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

#include "libhpx/debug.h"
#include "libhpx/parcel.h"

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
  return &p->data;

}


void parcel_init(hpx_parcel_t *p, int size) {
  p->stack = NULL;
  p->size  = size;
}


void parcel_fini(hpx_parcel_t *p) {
}


static void _set_next(hpx_parcel_t *p, hpx_parcel_t *next) {
  memcpy(&p->data, &next, sizeof(next));
}


static hpx_parcel_t *_get_next(hpx_parcel_t *p) {
  hpx_parcel_t *next = NULL;
  memcpy(&next, &p->data, sizeof(next));
  return next;
}


/// pop a single parcel off of the front of a parcel list
hpx_parcel_t *parcel_pop(hpx_parcel_t **parcel) {
  hpx_parcel_t *p = *parcel;
  if (!p)
    return NULL;

  *parcel = _get_next(p);
  return p;
}


/// just push the parcel onto the front of the list.
void parcel_push(hpx_parcel_t **list, hpx_parcel_t *p) {
  _set_next(p, *list);
  *list = p;
}


/// concatenate two parcel lists
void parcel_cat(hpx_parcel_t **list, hpx_parcel_t *p) {
  // find the end of p
  hpx_parcel_t *end = p;
  while (_get_next(end))
    end = _get_next(end);

  // update the two pointers
  _set_next(end, *list);
  *list = p;
}

