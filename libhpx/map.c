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
#include <libhpx/locality.h>
#include <libhpx/memory.h>
#include <libhpx/parcel.h>
#include <libhpx/scheduler.h>
#include <libhpx/worker.h>


typedef struct {
  uint32_t dst_stride;
  char data[0];
} _call_and_memput_args_t;

// This action allocates a local future, sends a parcel that's
// embedded in its payload with the local future as its continuation,
// and then copies the result into a remote global address.
static int
_call_and_memput_action_handler(_call_and_memput_args_t *args, size_t size) {
  hpx_parcel_t *parcel = parcel_clone((hpx_parcel_t*)args->data);
  dbg_assert(parcel);

  // replace the "dst" lco in the parcel's continuation addr with the
  // local future
  hpx_addr_t dst = parcel->c_target;
  hpx_addr_t local = hpx_lco_future_new(args->dst_stride);
  parcel->c_target = local;

  // send the parcel..
  hpx_parcel_send(parcel, HPX_NULL);

  void *buf;
  bool stack_allocated = true;
  if (hpx_thread_can_alloca(args->dst_stride) >= HPX_PAGE_SIZE) {
      buf = alloca(args->dst_stride);
  } else {
      stack_allocated = false;
      buf = registered_malloc(args->dst_stride);
  }

  hpx_lco_get(local, args->dst_stride, buf);
  hpx_lco_delete(local, HPX_NULL);

  // steal the current continuation
  hpx_parcel_t *parent = scheduler_current_parcel();
  hpx_gas_memput_lsync(dst, buf, args->dst_stride, parent->c_target);
  parent->c_target = HPX_NULL;

  if (!stack_allocated) {
      registered_free(buf);
  }
  return HPX_SUCCESS;
}
LIBHPX_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _call_and_memput_action,
              _call_and_memput_action_handler, HPX_POINTER, HPX_SIZE_T);

static int _va_map(hpx_action_t action, uint32_t n,
                   hpx_addr_t src, uint32_t src_stride,
                   hpx_addr_t dst, uint32_t dst_stride,
                   uint32_t bsize, hpx_addr_t sync, int nargs, va_list *vargs) {
  int e = HPX_SUCCESS;
  hpx_addr_t remote = HPX_NULL;
  if (sync) {
      remote = hpx_lco_and_new(n);
      e = hpx_call_when_with_continuation(remote, sync, hpx_lco_set_action,
                                          remote, hpx_lco_delete_action, NULL, 0);
      dbg_check(e, "could not chain LCO\n");
  }

  if (dst == HPX_NULL) {
    printf("process-map not implemented.\n");
    hpx_abort();
  }

  // the root of all evil: Use "dst" as the continuation address to
  // avoid creating another field in the args structure.
  hpx_parcel_t *p = action_create_parcel_va(src, action, dst, hpx_lco_set_action,
                                            nargs, vargs);
  size_t args_size = sizeof(_call_and_memput_args_t) + parcel_size(p);
  _call_and_memput_args_t *args = malloc(args_size);
  args->dst_stride = dst_stride;

  for (int i = 0; i < n; ++i) {
    // Update the target of the parcel..
    p->target = src;
    p->c_target = dst;

    // ..and put the new destination in the payload
    memcpy(args->data, p, parcel_size(p));
    e = hpx_call_with_continuation(src, _call_and_memput_action, remote,
                                   hpx_lco_set_action, args, args_size);
    dbg_check(e, "could not send parcel for map\n");

    src = hpx_addr_add(src, src_stride, bsize);
    dst = hpx_addr_add(dst, dst_stride, bsize);
  }
  hpx_parcel_release(p);
  return e;
}

int _hpx_map(hpx_action_t action, uint32_t n,
             hpx_addr_t src, uint32_t src_stride,
             hpx_addr_t dst, uint32_t dst_stride,
             uint32_t bsize, hpx_addr_t sync, int nargs, ...) {
  va_list vargs;
  va_start(vargs, nargs);
  int e = _va_map(action, n, src, src_stride, dst, dst_stride, bsize, sync,
                  nargs, &vargs);
  va_end(vargs);
  return e;
}

int _hpx_map_sync(hpx_action_t action, uint32_t n,
                  hpx_addr_t src, uint32_t src_stride,
                  hpx_addr_t dst, uint32_t dst_stride,
                  uint32_t bsize, int nargs, ...) {
  hpx_addr_t sync = hpx_lco_future_new(0);
  if (sync == HPX_NULL) {
    log_error("could not allocate an LCO.\n");
    return HPX_ENOMEM;
  }

  va_list vargs;
  va_start(vargs, nargs);
  int e = _va_map(action, n, src, src_stride, dst, dst_stride, bsize, sync,
                  nargs, &vargs);
  va_end(vargs);

  if (HPX_SUCCESS != hpx_lco_wait(sync)) {
    dbg_error("failed map\n");
  }

  hpx_lco_delete(sync, HPX_NULL);
  return e;
}

