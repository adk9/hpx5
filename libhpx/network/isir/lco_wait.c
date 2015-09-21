// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <alloca.h>
#include <string.h>
#include "libhpx/action.h"
#include "libhpx/debug.h"
#include "libhpx/gpa.h"
#include "libhpx/parcel.h"
#include "libhpx/scheduler.h"
#include "isir.h"

static int _isir_lco_wait_handler(int reset) {
  int e = HPX_SUCCESS;

  if (reset) {
    e = hpx_lco_wait_reset(self->current->target);
  }
  else {
    e = hpx_lco_wait(self->current->target);
  }

  return e;
}
static LIBHPX_ACTION(HPX_DEFAULT, 0, _isir_lco_wait, _isir_lco_wait_handler,
                     HPX_INT);

typedef struct {
  hpx_addr_t lco;
  int reset;
} _isir_lco_wait_env_t;

static void _isir_lco_wait_continuation(hpx_parcel_t *p, void *env) {
  _isir_lco_wait_env_t *e = env;
  hpx_addr_t gpa = offset_to_gpa(here->rank, (uint64_t)(uintptr_t)p);
  hpx_parcel_t *q = action_create_parcel(e->lco, _isir_lco_wait, gpa,
                                         resume_parcel, 1, &e->reset);
  dbg_assert(q);
  parcel_launch(q);
}

int isir_lco_wait(void *obj, hpx_addr_t lco, int reset) {
  _isir_lco_wait_env_t env = {
    .lco = lco,
    .reset = reset
  };
  scheduler_suspend(_isir_lco_wait_continuation, &env, 0);
  return HPX_SUCCESS;
}
