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
#include "config.h"
#endif

/// @file libhpx/scheduler/user_lco.c
/// @brief A user-defined LCO.

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/scheduler.h"
#include "cvar.h"
#include "lco.h"

/// Generic LCO interface.
/// @{
typedef struct {
  lco_t                 lco;
  cvar_t               cvar;
  hpx_monoid_id_t        id;
  hpx_monoid_op_t        op;
  hpx_predicate_t predicate;
  void                 *buf;
} _user_lco_t;

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
  libhpx_global_free(lco);
}

/// Handle an error condition.
static void _user_lco_error(lco_t *lco, hpx_status_t code) {
  _user_lco_t *u = (_user_lco_t *)lco;
  lco_lock(lco);
  scheduler_signal_error(&u->cvar, code);
  lco_unlock(lco);
}

static void _user_lco_reset(lco_t *lco) {
  _user_lco_t *u = (_user_lco_t *)lco;
  lco_lock(lco);
  dbg_assert_str(cvar_empty(&u->cvar),
                 "Reset on an LCO that has waiting threads.\n");
  cvar_reset(&u->cvar);
  lco_unlock(lco);
}

/// Invoke an operation on the user-defined LCO's buffer.
static void _user_lco_set(lco_t *lco, int size, const void *from) {
  _user_lco_t *u = (_user_lco_t *)lco;

  lco_lock(lco);
  // perform the op()

  u->op(u->buf, from, size);
  if (u->predicate(u->buf, size)) {
    scheduler_signal_all(&u->cvar);
  }
  lco_unlock(lco);
}

/// Get the user-defined LCO's buffer.
static hpx_status_t _user_lco_get(lco_t *lco, int size, void *out) {
  _user_lco_t *u = (_user_lco_t *)lco;
  hpx_status_t status = HPX_SUCCESS;
  lco_lock(lco);

  while (!u->predicate(u->buf, size) && (status == HPX_SUCCESS)) {
    status = scheduler_wait(&lco->lock, &u->cvar);
  }

  // if there was an error signal, unlock and return it
  if (status != HPX_SUCCESS) {
    lco_unlock(lco);
    return status;
  }

  // copy out the value if the caller wants it
  if (size && out) {
    memcpy(out, u->buf, size);
  }

  lco_unlock(lco);
  return status;
}

// Wait for the reduction.
static hpx_status_t _user_lco_wait(lco_t *lco) {
  return _user_lco_get(lco, 0, NULL);
}

static void _user_lco_init(_user_lco_t *u, size_t size, hpx_monoid_id_t id,
                           hpx_monoid_op_t op, hpx_predicate_t predicate) {
  // vtable
  static const lco_class_t vtable = {
    .on_fini     = _user_lco_fini,
    .on_error    = _user_lco_error,
    .on_set      = _user_lco_set,
    .on_attach   = NULL,
    .on_get      = _user_lco_get,
    .on_getref   = NULL,
    .on_release  = NULL,
    .on_wait     = _user_lco_wait,
    .on_reset    = _user_lco_reset,
    .on_size     = _user_lco_size
  };

  assert(id);
  assert(op);
  assert(predicate);

  lco_init(&u->lco, &vtable);
  cvar_reset(&u->cvar);
  u->op = op;
  u->id = id;
  u->predicate = predicate;
  u->buf = NULL;

  if (size) {
    u->buf = malloc(size);
    assert(u->buf);
  }

  u->id(u->buf, size);
}
/// @}

hpx_addr_t hpx_lco_user_new(size_t size, hpx_monoid_id_t id, hpx_monoid_op_t op,
                            hpx_predicate_t predicate) {
  _user_lco_t *u = libhpx_global_malloc(sizeof(*u));
  assert(u);
  _user_lco_init(u, size, id, op, predicate);
  return lva_to_gva(u);
}

typedef struct {
  int                     n;
  size_t               size;
  hpx_monoid_id_t        id;
  hpx_monoid_op_t        op;
  hpx_predicate_t predicate;
} _user_array_args_t;

/// Initialize a block of array of lco.
static HPX_PINNED(_block_local_init, const _user_array_args_t *args) {
  _user_array_args_t *lco = hpx_thread_current_local_target();
  dbg_assert(lco);

  for (int i = 0; i < args->n; i++) {
    void *addr = (void *)((uintptr_t)lco + i * (sizeof(_user_lco_t) + args->size));
    _user_lco_init(addr, args->size, args->id, args->op, args->predicate);
  }

  return HPX_SUCCESS;
}

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
hpx_addr_t hpx_lco_user_local_array_new(int n, size_t size,
                                        hpx_monoid_id_t id, hpx_monoid_op_t op,
                                        hpx_predicate_t predicate) {
  _user_array_args_t args;
  uint32_t lco_bytes = sizeof(_user_lco_t) + size;
  dbg_assert(n * lco_bytes < UINT32_MAX);
  uint32_t  block_bytes = n * lco_bytes;
  hpx_addr_t base = hpx_gas_alloc(block_bytes);

  args.n = n;
  args.size = size;
  args.id   = id;
  args.op   = op;
  args.predicate = predicate;
  int e = hpx_call_sync(base, _block_local_init, NULL, 0, &args, sizeof(args));
  dbg_check(e, "call of _block_init_action failed\n");

  // return the base address of the allocation
  return base;
}
