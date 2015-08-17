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
#include "libhpx/parcel.h"
#include "libhpx/scheduler.h"
#include "isir.h"

typedef struct {
  hpx_parcel_t *p;
  void *out;
  char data[];
} _isir_lco_get_reply_args_t;

static int
_isir_lco_get_reply_handler(_isir_lco_get_reply_args_t *args, size_t n) {
  size_t bytes = n - sizeof(*args);
  if (bytes) {
    memcpy(args->out, args->data, bytes);
  }
  scheduler_spawn(args->p);
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_INTERRUPT, HPX_MARSHALLED, _isir_lco_get_reply,
                     _isir_lco_get_reply_handler, HPX_POINTER, HPX_SIZE_T);

static int
_isir_lco_get_request_handler(hpx_parcel_t *p, size_t n, void *out, int reset) {
  dbg_assert(n > 0);

  // eagerly create a continuation parcel so that we can serialize the data into
  // it directly without an extra copy
  size_t bytes = sizeof(_isir_lco_get_reply_args_t) + n;
  hpx_parcel_t *curr = self->current;
  hpx_parcel_t *cont = parcel_new(curr->c_target, curr->c_action, 0, 0,
                                  curr->pid, NULL, bytes);
  curr->c_target = 0;
  curr->c_action = 0;

  // forward the parcel and output buffer back to the sender
  _isir_lco_get_reply_args_t *args = hpx_parcel_get_data(cont);
  args->p = p;
  args->out = out;

  // perform the blocking get operation
  int e = HPX_SUCCESS;
  if (reset) {
    e = hpx_lco_get_reset(curr->target, n, args->data);
  }
  else {
    e = hpx_lco_get(curr->target, n, args->data);
  }

  // send the continuation
  parcel_launch_error(cont, e);

  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, 0, _isir_lco_get_request,
                     _isir_lco_get_request_handler, HPX_POINTER, HPX_SIZE_T,
                     HPX_POINTER, HPX_INT);

typedef struct {
  hpx_addr_t lco;
  size_t n;
  void *out;
  int reset;
} _lco_get_env_t;

static void _lco_get_continuation(hpx_parcel_t *p, void *env) {
  _lco_get_env_t *e = env;
  hpx_parcel_t *l = action_create_parcel(e->lco, _isir_lco_get_request,
                                         HPX_HERE, _isir_lco_get_reply,
                                         4, &p, &e->n, &e->out, &e->reset);
  parcel_launch(l);
}

int isir_lco_get(void *obj, hpx_addr_t lco, size_t n, void *out, int reset) {
  _lco_get_env_t env = {
    .lco = lco,
    .n = n,
    .out = out,
    .reset = reset
  };

  scheduler_suspend(_lco_get_continuation, &env, 0);
  return HPX_SUCCESS;
}
