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

#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libhpx/parcel.h>
#include <libhpx/scheduler.h>
#include "commands.h"
#include "pwc.h"
#include "xport.h"

/// This acts as a parcel_suspend transfer to allow _pwc_lco_get_request_handler
/// to wait for its pwc to complete.
static int
_pwc(void *op) {
  pwc_network_t *pwc = (pwc_network_t*)here->network;
  return pwc->xport->pwc(op);
}

typedef struct {
  hpx_parcel_t *p;
  size_t n;
  void *out;
  xport_key_t key;
  int rank;
} _pwc_lco_get_request_args_t;

static int
_pwc_lco_get_request_handler(_pwc_lco_get_request_args_t *args, size_t n) {
  dbg_assert(n > 0);

  pwc_network_t *pwc = (pwc_network_t*)here->network;
  hpx_addr_t lco = hpx_thread_current_target();
  void *ref;
  int e = hpx_lco_getref(lco, args->n, &ref);
  dbg_check(e, "Failed getref during remote lco get request.\n");

  // Create the transport operation to perform the rdma put operation, along
  // with the remote command to restart the waiting thread and the local
  // completion command to resume the current thread.
  xport_op_t op = {
    .rank = args->rank,
    .n = args->n,
    .dest = args->out,
    .dest_key = args->key,
    .src = ref,
    .src_key = pwc->xport->key_find_ref(pwc->xport, ref, args->n),
    .lop = command_pack(resume_parcel, (uintptr_t)self->current),
    .rop = command_pack(resume_parcel, (uintptr_t)args->p)
  };
  dbg_assert_str(op.src_key, "LCO reference must point to registered memory\n");

  // Issue the pwc and wait for synchronous local completion so that the ref
  // buffer doesn't move during the underlying rdma, if there is any
  e = scheduler_suspend(_pwc, &op);

  hpx_lco_release(lco, ref);
  dbg_check(e, "Failed to start rendezvous put during remote get operation\n");
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _pwc_lco_get_request,
                  _pwc_lco_get_request_handler, HPX_POINTER, HPX_SIZE_T);


int
pwc_lco_get(void *obj, hpx_addr_t lco, size_t n, void *out) {
  pwc_network_t *pwc = (pwc_network_t*)here->network;

  _pwc_lco_get_request_args_t args = {
    .p = scheduler_current_parcel(),
    .n = n,
    .out = out,
    .rank = here->rank,
    .key = {0}
  };

  // If the output buffer is already registered, then we just need to copy the
  // key into the args structure, otherwise we need to register the region.
  const void *key = pwc->xport->key_find_ref(pwc->xport, out, n);
  if (key) {
    pwc->xport->key_copy(&args.key, key);
  }
  else {
    pwc->xport->pin(out, n, &args.key);
  }

  hpx_parcel_t *p = parcel_create(lco, _pwc_lco_get_request, HPX_NULL,
                                  HPX_ACTION_NULL, 2, &args, sizeof(args));
  int e = scheduler_suspend((int (*)(void*))parcel_launch, p);

  // If we registered the output buffer dynamically, then we need to de-register
  // it now.
  if (!key) {
    pwc->xport->unpin(out, n);
  }
  return e;
}
