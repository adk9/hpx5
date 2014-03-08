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

hpx_parcel_t *
hpx_parcel_acquire(size_t size) {
  hpx_parcel_t *p = malloc(sizeof(*p) + size);

  p->next   = NULL;
  p->data   = (size) ? &p->payload : NULL;

  p->size   = size;
  p->action = HPX_ACTION_NULL;
  p->target = HPX_HERE;
  p->cont   = HPX_NULL;
  memset(&p->payload, 0, size);

  return p;
}


void
hpx_parcel_release(hpx_parcel_t *p) {
  free(p);
}


void
hpx_parcel_set_action(hpx_parcel_t *p, const hpx_action_t action) {
  assert(p);
  p->action = action;
}


void
hpx_parcel_set_target(hpx_parcel_t *p, const hpx_addr_t addr) {
  assert(p);
  p->target = addr;
}


void
hpx_parcel_set_cont(hpx_parcel_t *p, const hpx_addr_t cont) {
  assert(p);
  p->cont = cont;
}


void
hpx_parcel_set_data(hpx_parcel_t *p, const void *data, int size) {
  assert(p);
  if (size)
    memcpy(p->data, data, size);
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
  assert(p);
  return p->data;
}

