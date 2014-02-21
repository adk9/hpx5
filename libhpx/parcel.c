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
/// ----------------------------------------------------------------------------
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "hpx.h"
#include "parcel.h"
#include "network.h"

int
parcel_init_module(void) {
  return HPX_SUCCESS;
}

int
parcel_init_thread(void) {
  return HPX_SUCCESS;
}

void
parcel_fini_module(void) {
}

void
parcel_fini_thread(void) {
}


void
parcel_release(hpx_parcel_t *parcel) {
  free(parcel);
}

hpx_parcel_t *
hpx_parcel_acquire(unsigned size) {
  hpx_parcel_t *p = malloc(sizeof(*p) + size);

  p->next   = NULL;
  p->thread = NULL;
  p->data   = (size) ? &p->payload : NULL;

  p->size   = size;
  p->action = HPX_ACTION_NULL;
  p->target = HPX_NULL;
  p->cont   = HPX_NULL;
  memset(&p->payload, 0, size);

  return p;
}

void
hpx_parcel_set_action(hpx_parcel_t *p, hpx_action_t action) {
  assert(p);
  p->action = action;
}

void
hpx_parcel_set_target(hpx_parcel_t *p, hpx_addr_t addr) {
  assert(p);
  p->target = addr;
}

void
hpx_parcel_set_cont(hpx_parcel_t *p, hpx_addr_t cont) {
  assert(p);
  p->cont = HPX_NULL;
}

void *
hpx_parcel_get_data(hpx_parcel_t *p) {
  assert(p);
  return p->data;
}
