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


/// @file libhpx/scheduler/sema.c
/// @brief Implements the semaphore LCO.

#include <assert.h>
#include <stdlib.h>
#include <string.h>


#include "hpx/hpx.h"
#include "libhpx/scheduler.h"
#include "libhpx/locality.h"
#include "cvar.h"
#include "lco.h"


/// Local gencount interface.
/// @{
typedef struct {
  lco_t              lco;
  cvar_t           oflow;
  unsigned long      gen;
  unsigned long ninplace;
  cvar_t       inplace[];
} _gencount_t;


static hpx_action_t _gencount_wait_gen_action = 0;


static void _gencount_fini(lco_t *lco) {
  if (!lco)
    return;

  _gencount_t *gencnt = (_gencount_t *)lco;
  lco_lock(&gencnt->lco);
  libhpx_global_free(gencnt);
}


static void _gencount_error(lco_t *lco, hpx_status_t code) {
  _gencount_t *gen = (_gencount_t *)lco;
  lco_lock(&gen->lco);

  for (unsigned i = 0, e = gen->ninplace; i < e; ++i)
    scheduler_signal_error(&gen->inplace[i], code);
  scheduler_signal_error(&gen->oflow, code);

  lco_unlock(&gen->lco);
}


/// Set is equivalent to incrementing the generation count
static void _gencount_set(lco_t *lco, int size, const void *from) {
  _gencount_t *gencnt = (_gencount_t *)lco;
  lco_lock(&gencnt->lco);
  unsigned long gen = gencnt->gen++;
  scheduler_signal_all(&gencnt->oflow);

  if (gencnt->ninplace > 0) {
    cvar_t *cvar = &gencnt->inplace[gen % gencnt->ninplace];
    scheduler_signal_all(cvar);
  }
  lco_unlock(&gencnt->lco);
}


/// Get returns the current generation, it does not block.
static hpx_status_t _gencount_get(lco_t *lco, int size, void *out) {
  _gencount_t *gencnt = (_gencount_t *)lco;
  lco_lock(&gencnt->lco);
  if (size)
    memcpy(out, &gencnt->gen, size);
  lco_unlock(&gencnt->lco);
  return HPX_SUCCESS;
}


// Wait means to wait for one generation, i.e., wait on the next generation. We
// actually just wait on oflow since we signal that every time the generation
// changes.
static hpx_status_t _gencount_wait(lco_t *lco) {
  _gencount_t *gencnt = (_gencount_t *)lco;
  lco_lock(&gencnt->lco);
  hpx_status_t status = scheduler_wait(&gencnt->lco.lock, &gencnt->oflow);
  lco_unlock(&gencnt->lco);
  return status;
}


// Wait for a specific generation.
static hpx_status_t _gencount_wait_gen(_gencount_t *gencnt, unsigned long gen) {
  hpx_status_t status = HPX_SUCCESS;
  lco_lock(&gencnt->lco);

  // while this generation is in the future, wait on the appropriate condition
  unsigned long current = gencnt->gen;
  while (current < gen && status == HPX_SUCCESS) {
    cvar_t *cond;
    if (gen < current + gencnt->ninplace)
      cond = &gencnt->inplace[gen % gencnt->ninplace];
    else
      cond = &gencnt->oflow;

    status = scheduler_wait(&gencnt->lco.lock, cond);
    current = gencnt->gen;
  }

  lco_unlock(&gencnt->lco);
  return status;
}


static void _gencount_init(_gencount_t *gencnt, unsigned long ninplace) {
  static const lco_class_t gencount_vtable = {
    _gencount_fini,
    _gencount_error,
    _gencount_set,
    _gencount_get,
    _gencount_wait
  };

  lco_init(&gencnt->lco, &gencount_vtable, 0);
  cvar_reset(&gencnt->oflow);
  gencnt->gen = 0;
  gencnt->ninplace = ninplace;
  for (unsigned long i = 0, e = ninplace; i < e; ++i)
    cvar_reset(&gencnt->inplace[i]);
}

static hpx_status_t _gencount_wait_gen_proxy(unsigned long *gen) {
  hpx_addr_t target = hpx_thread_current_target();
  return hpx_lco_gencount_wait(target, *gen);
}


static HPX_CONSTRUCTOR void _initialize_actions(void) {
  _gencount_wait_gen_action = HPX_REGISTER_ACTION(_gencount_wait_gen_proxy);
}

hpx_addr_t
hpx_lco_gencount_new(unsigned long ninplace) {
  _gencount_t *cnt = libhpx_global_malloc(sizeof(*cnt) +
                                          ninplace * sizeof(cvar_t));
  assert(cnt);
  _gencount_init(cnt, ninplace);
  return lva_to_gva(cnt);
}

void hpx_lco_gencount_inc(hpx_addr_t gencnt, hpx_addr_t rsync) {
  hpx_lco_set(gencnt, 0, NULL, HPX_NULL, rsync);
}


hpx_status_t hpx_lco_gencount_wait(hpx_addr_t gencnt, unsigned long gen) {
  _gencount_t *local;
  if (!hpx_gas_try_pin(gencnt, (void**)&local))
    return hpx_call_sync(gencnt, _gencount_wait_gen_action, &gen, gen, NULL, 0);

  hpx_status_t status = _gencount_wait_gen(local, gen);
  hpx_gas_unpin(gencnt);
  return status;
}
