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

/// @file libhpx/scheduler/user_lco.c
/// @brief A user-defined LCO.
#include <assert.h>
#include <string.h>

#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libhpx/locality.h>
#include <libhpx/memory.h>
#include <libhpx/scheduler.h>
#include "cvar.h"
#include "lco.h"

/// Generic LCO interface.
/// @{
typedef struct {
  lco_t              lco;
  cvar_t            cvar;
  size_t            size;
  hpx_action_t        id;
  hpx_action_t        op;
  hpx_action_t predicate;
  void              *buf;
} _user_lco_t;


// Forward declaration---used during reset as well.
static int _user_lco_init_handler(_user_lco_t *u, size_t size, hpx_action_t id,
                                  hpx_action_t op, hpx_action_t predicate);

static bool _trigger(_user_lco_t *u) {
  if (lco_get_triggered(&u->lco)) {
    return false;
  }
  lco_set_triggered(&u->lco);
  return true;
}

static size_t _user_lco_size(lco_t *lco) {
  _user_lco_t *user_lco = (_user_lco_t *)lco;
  return sizeof(*user_lco);
}

/// Deletes a user-defined LCO.
static void _user_lco_fini(lco_t *lco) {
  if (!lco) {
    return;
  }

  lco_lock(lco);
  _user_lco_t *u = (_user_lco_t *)lco;
  if (u->buf) {
    free(u->buf);
  }
  lco_fini(lco);
}

/// Handle an error condition.
static void _user_lco_error(lco_t *lco, hpx_status_t code) {
  lco_lock(lco);
  _user_lco_t *u = (_user_lco_t *)lco;
  _trigger(u);
  scheduler_signal_error(&u->cvar, code);
  lco_unlock(lco);
}

static void _user_lco_reset(lco_t *lco) {
  _user_lco_t *u = (_user_lco_t *)lco;
  lco_lock(lco);
  cvar_clear_error(&u->cvar);
  dbg_assert_str(cvar_empty(&u->cvar),
                 "Reset on an LCO that has waiting threads.\n");
  lco_reset_triggered(&u->lco);
  _user_lco_init_handler(u, u->size, u->id, u->op, u->predicate);
  lco_unlock(lco);
}

/// Invoke an operation on the user-defined LCO's buffer.
static void _user_lco_set(lco_t *lco, int size, const void *from) {
  lco_lock(lco);
  _user_lco_t *u = (_user_lco_t *)lco;

  if (lco_get_triggered(&u->lco)) {
    dbg_error("cannot set an already set user_lco.\n");
    goto unlock;
  }

  // perform the op()
  hpx_action_handler_t f = 0;
  f = action_table_get_handler(here->actions, u->op);
  hpx_monoid_op_t op = (hpx_monoid_op_t)f;
  op(u->buf, from, size);

  f = action_table_get_handler(here->actions, u->predicate);
  hpx_predicate_t predicate = (hpx_predicate_t)f;
  if (predicate(u->buf, size)) {
    lco_set_triggered(&u->lco);
    scheduler_signal_all(&u->cvar);
  }

  unlock:
   lco_unlock(lco);
}

static hpx_status_t _user_lco_attach(lco_t *lco, hpx_parcel_t *p) {
  hpx_status_t status = HPX_SUCCESS;
  lco_lock(lco);
  _user_lco_t *u = (_user_lco_t *)lco;

  if (!lco_get_triggered(lco)) {
    status = cvar_attach(&u->cvar, p);
    goto unlock;
  }

  // If there was and error, then return that error without sending the parcel
  status = cvar_get_error(&u->cvar);
  if (status != HPX_SUCCESS) {
    goto unlock;
  }

  // go ahead and send this parcel eagerly
  hpx_parcel_send(p, HPX_NULL);

 unlock:
  lco_unlock(lco);
  return status;
}

static hpx_status_t _wait(_user_lco_t *u) {
  if (!lco_get_triggered(&u->lco))
    return scheduler_wait(&u->lco.lock, &u->cvar);

  return cvar_get_error(&u->cvar);
}

/// Get the user-defined LCO's buffer.
static hpx_status_t _user_lco_get(lco_t *lco, int size, void *out) {
  hpx_status_t status = HPX_SUCCESS;
  lco_lock(lco);

  _user_lco_t *u = (_user_lco_t *)lco;

  status = _wait(u);
  // copy out the value if the caller wants it
  if ((status == HPX_SUCCESS) && out) {
    memcpy(out, u->buf, size);
  }

  lco_unlock(lco);
  return status;
}

// Wait for the reduction.
static hpx_status_t _user_lco_wait(lco_t *lco) {
  hpx_status_t status = HPX_SUCCESS;
  lco_lock(lco);
  _user_lco_t *u = (_user_lco_t *)lco;
  status = _wait(u);
  lco_unlock(lco);
  return status;
}

// vtable
static const lco_class_t _user_lco_vtable = {
  .on_fini     = _user_lco_fini,
  .on_error    = _user_lco_error,
  .on_set      = _user_lco_set,
  .on_attach   = _user_lco_attach,
  .on_get      = _user_lco_get,
  .on_getref   = NULL,
  .on_release  = NULL,
  .on_wait     = _user_lco_wait,
  .on_reset    = _user_lco_reset,
  .on_size     = _user_lco_size
};

static int
_user_lco_init_handler(_user_lco_t *u, size_t size, hpx_action_t id,
                       hpx_action_t op, hpx_action_t predicate) {
  assert(id);
  assert(op);
  assert(predicate);

  lco_init(&u->lco, &_user_lco_vtable);
  cvar_reset(&u->cvar);
  u->size = size;
  u->op = op;
  u->id = id;
  u->predicate = predicate;
  if (u->buf) {
    free(u->buf);
  }

  if (u->size) {
    u->buf = malloc(u->size);
    assert(u->buf);
  }

  hpx_action_handler_t f = action_table_get_handler(here->actions, u->id);
  hpx_monoid_id_t lid = (hpx_monoid_id_t)f;
  lid(u->buf, u->size);

  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, HPX_PINNED, _user_lco_init_async,
                  _user_lco_init_handler, HPX_POINTER, HPX_SIZE_T,
                  HPX_ACTION_T, HPX_ACTION_T, HPX_ACTION_T);
/// @}

hpx_addr_t hpx_lco_user_new(size_t size, hpx_action_t id, hpx_action_t op,
                            hpx_action_t predicate) {
  _user_lco_t *u = NULL;
  hpx_addr_t gva = hpx_gas_calloc_local(1, sizeof(*u), 0);
  LCO_LOG_NEW(gva);

  if (!hpx_gas_try_pin(gva, (void**)&u)) {
    int e = hpx_call_sync(gva, _user_lco_init_async, NULL, 0, &size, &id, &op,
                          &predicate);
    dbg_check(e, "could not initialize an allreduce at %lu\n", gva);
  }
  else {
    _user_lco_init_handler(u, size, id, op, predicate);
    hpx_gas_unpin(gva);
  }

  return gva;
}

/// Initialize a block of array of lco.
static int
_block_local_init_handler(void *lco, int n, size_t size, hpx_action_t id,
                          hpx_action_t op, hpx_action_t predicate) {
  for (int i = 0; i < n; i++) {
    void *addr = (void *)((uintptr_t)lco + i * (sizeof(_user_lco_t) + size));
    _user_lco_init_handler(addr, size, id, op, predicate);
  }
  return HPX_SUCCESS;
}

static HPX_ACTION(HPX_DEFAULT, HPX_PINNED, _block_local_init,
                  _block_local_init_handler, HPX_POINTER, HPX_INT, HPX_SIZE_T,
                  HPX_POINTER, HPX_POINTER, HPX_POINTER);

/// Allocate an array of user LCO local to the calling locality.
/// @param          n The (total) number of lcos to allocate
/// @param       size The size of the LCO Buffer
/// @param         id An initialization function for the data, this is
///                   used to initialize the data in every epoch.
/// @param         op The commutative-associative operation we're
///                   performing.
/// @param  predicate Predicate to guard the LCO.
///
/// @returns the global address of the allocated array lco.
hpx_addr_t hpx_lco_user_local_array_new(int n, size_t size, hpx_action_t id,
                                        hpx_action_t op, hpx_action_t predicate)
{
  uint32_t lco_bytes = sizeof(_user_lco_t) + size;
  dbg_assert(n * lco_bytes < UINT32_MAX);
  uint32_t  block_bytes = n * lco_bytes;
  hpx_addr_t base = hpx_gas_alloc_local(block_bytes, 0);

  int e = hpx_call_sync(base, _block_local_init, NULL, 0, &n, &size, &id, &op, &predicate);
  dbg_check(e, "call of _block_init_action failed\n");

  // return the base address of the allocation
  return base;
}
