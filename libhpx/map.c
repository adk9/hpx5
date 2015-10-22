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

#include <hpx/hpx.h>
#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libhpx/locality.h>
#include <libhpx/memory.h>
#include <libhpx/parcel.h>
#include <libhpx/scheduler.h>
#include <libhpx/worker.h>

static int _va_map_cont(hpx_action_t action, hpx_addr_t base, int n,
                        size_t offset, size_t bsize, hpx_action_t rop,
                        hpx_addr_t raddr,int nargs, va_list *vargs) {
  hpx_addr_t and = hpx_lco_and_new(n);
  for (int i = 0; i < n; ++i) {
    va_list temp;
    va_copy(temp, *vargs);
    hpx_addr_t element = hpx_addr_add(base, i * bsize + offset, bsize);
    int e = action_call_va(element, action, raddr, rop, and, HPX_NULL,
                           nargs, &temp);
    dbg_check(e, "failed to call action\n");
    va_end(temp);
  }
  int e = hpx_lco_wait(and);
  hpx_lco_delete(and, HPX_NULL);
  return e;
}

int _hpx_map_with_continuation(hpx_action_t action, hpx_addr_t base, int n,
                               size_t offset, size_t bsize, hpx_action_t rop,
                               hpx_addr_t raddr,int nargs, ...) {
  va_list vargs;
  va_start(vargs, nargs);
  int e = _va_map_cont(action, base, n, offset, bsize, rop, raddr, nargs,
                         &vargs);
  dbg_check(e, "failed _hpx_map_with_continuation\n");
  va_end(vargs);
  return e;
}


// This action allocates a local future, sends a parcel that's
// embedded in its payload with the local future as its continuation,
// and then copies the result into a remote global address.
static int
_call_and_memput_action_handler(hpx_parcel_t *parcel, size_t size) {
  hpx_parcel_t *parent = scheduler_current_parcel();
  parcel_state_t state = parcel_get_state(parent);
  dbg_assert(!parcel_retained(state));
  state |= PARCEL_RETAINED;
  parcel_set_state(parent, state);

  dbg_assert(parcel);
  state = parcel_get_state(parcel);
  dbg_assert(!parcel_nested(state));
  state |= PARCEL_NESTED;

  // replace the "dst" lco in the parcel's continuation addr with the
  // local future
  hpx_addr_t dst = parcel->c_target;
  uint64_t stride = (uintptr_t)parcel->next;
  hpx_addr_t local = hpx_lco_future_new(stride);
  parcel->c_target = local;

  // send the parcel..
  hpx_parcel_send(parcel, HPX_NULL);

  void *buf;
  bool stack_allocated = true;
  if (hpx_thread_can_alloca(stride) >= HPX_PAGE_SIZE) {
    buf = alloca(stride);
  } else {
    stack_allocated = false;
    buf = registered_malloc(stride);
  }

  hpx_lco_get(local, stride, buf);
  hpx_lco_delete(local, HPX_NULL);

  // steal the current continuation
  hpx_gas_memput_lsync(dst, buf, stride, parent->c_target);
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
    return _va_map_cont(action, src, n, src_stride, bsize, HPX_ACTION_NULL,
                        HPX_NULL, nargs, vargs);
  }

  // hack: Use "dst" as the continuation address to avoid creating
  // another field in the args structure.
  hpx_parcel_t *p = action_create_parcel_va(src, action, dst, hpx_lco_set_action,
                                            nargs, vargs);
  size_t size = parcel_size(p);
  hpx_parcel_t *args = malloc(size);
  // hack: use the parcel's "next" parameter to pass the output
  // stride.
  args->next = (void*)(uintptr_t)(dst_stride);

  for (int i = 0; i < n; ++i) {
    // Update the target of the parcel..
    p->target = src;
    p->c_target = dst;

    // ..and put the new destination in the payload
    memcpy(args, p, size);
    e = hpx_call_with_continuation(src, _call_and_memput_action, remote,
                                   hpx_lco_set_action, args, size);
    dbg_check(e, "could not send parcel for map\n");

    src = hpx_addr_add(src, src_stride, bsize);
    dst = hpx_addr_add(dst, dst_stride, bsize);
  }
  hpx_parcel_release(p);
  return e;
}

int _hpx_gas_map(hpx_action_t action, uint32_t n,
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

int _hpx_gas_map_sync(hpx_action_t action, uint32_t n,
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
