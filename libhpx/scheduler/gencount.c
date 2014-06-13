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
/// @file libhpx/scheduler/sema.c
/// @brief Implements the semaphore LCO.
/// ----------------------------------------------------------------------------
#include <assert.h>
#include <string.h>
#include "hpx/hpx.h"
#include "libhpx/scheduler.h"
#include "lco.h"

/// ----------------------------------------------------------------------------
/// Local gencount interface.
/// ----------------------------------------------------------------------------
/// @{
typedef struct {
  lco_t lco;
  unsigned long gen;
  unsigned int depth;
  lco_t generations[];
} _gencount_t;


/// Delete a gencount LCO, and all of its in-place generations.
static void
_delete(_gencount_t *gencnt) {
  if (!gencnt)
    return;
  lco_lock(&gencnt->lco);
  for (int i = 0, e = gencnt->depth; i < e; ++i) {
    lco_lock(&gencnt->generations[i]);
    lco_fini(&gencnt->generations[i]);
  }
  lco_fini(&gencnt->lco);
}

/// Get returns the current generation.
static hpx_status_t
_get(_gencount_t *gencnt, int size, void *out) {
  lco_lock(&gencnt->lco);
  if (size) {
    assert(out);
    assert(size == sizeof(gencnt->gen));
    memcpy(out, &gencnt->gen, size);
  }
  lco_unlock(&gencnt->lco);
  return HPX_SUCCESS;
}

/// Set is equivalent to incrementing the gencnt.
static void
_set(_gencount_t *gencnt, int size, const void *from, hpx_status_t status,
     hpx_addr_t sync)
{
  lco_lock(&gencnt->lco);
  unsigned long gen = gencnt->gen++;
  scheduler_signal(&gencnt->lco, HPX_SUCCESS);
  lco_reset(&gencnt->lco);
  if (gencnt->depth != 0) {
    lco_t *bucket = &gencnt->generations[gen % gencnt->depth];
    lco_lock(bucket);
    scheduler_signal(bucket, HPX_SUCCESS);
    lco_reset(bucket);
    lco_unlock(bucket);
  }
  lco_unlock(&gencnt->lco);
}

static lco_class_t _vtable = LCO_CLASS_INIT(_delete, _set, _get);


static void _init(_gencnt_t *gencnt, unsigned int depth) {
  lco_init(&gencnt->lco, &_vtable, 0);
  gencnt->count = 0;
  gencnt->depth = depth;
  for (int i = 0; i < depth; ++i) {
  }
}
