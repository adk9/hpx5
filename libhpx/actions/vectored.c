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

#include <string.h>
#include <hpx/hpx.h>
#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libhpx/parcel.h>
#include <libhpx/padding.h>
#include "init.h"

static void _pack_vectored(const void *obj, hpx_parcel_t *p, int n,
                           va_list *vargs) {
  n >>= 1;
  void *buffer = hpx_parcel_get_data(p);
  *(int*)buffer = n;
  size_t *sizes = (size_t*)((char*)buffer + sizeof(int));
  void *args = (char*)sizes + (sizeof(size_t) * n);

  size_t bytes = ALIGN(args - buffer, 8);
  for (int i = 0; i < n; ++i) {
    void *arg = va_arg(*vargs, void*);
    size_t size = va_arg(*vargs, int);
    sizes[i] = size;
    memcpy(args+bytes, arg, size);
    bytes += size + ALIGN(size, 8);
  }
}

static hpx_parcel_t *_new_vectored(const void *obj, hpx_addr_t addr,
                                   hpx_addr_t c_addr, hpx_action_t c_action,
                                   int n, va_list *args) {
  dbg_assert(n && args);
  dbg_assert_str(!(n & 1), "Vectored actions require even arg count: %d\n", n);

  const action_t *action = obj;
  hpx_action_t id = *action->id;
  hpx_pid_t pid = hpx_thread_current_pid();

  // For vectored arguments we want to read through the argument pairs and
  // accumuluate the total number of bytes that we'll need to allocate.  We need
  // `int` bytes for the number of tuples, then `n/2` `size_t` bytes for the
  // size array, then padding to align the first buffer, and then 8-byte aligned
  // sizes for each buffer.
  int ntuples = n >> 1;
  size_t bytes = sizeof(int) + ntuples * sizeof(size_t);

  // We will need to iterate the va_args again, so we make a copy, and position
  // its starting location on the first size. Then we just go through the list,
  // checking every other list element for the next size.
  va_list temp;
  va_copy(temp, *args);
  va_arg(temp, void*);
  for (int i = 0, e = ntuples; i < e; ++i, va_arg(temp, void*)) {
    bytes += ALIGN(bytes, 8);
    bytes += va_arg(temp, int);
  }
  va_end(temp);

  hpx_parcel_t *p = parcel_new(addr, id, c_addr, c_action, pid, NULL, bytes);
  _pack_vectored(obj, p, n, args);
  return p;
}

static int _exec_pinned_vectored(const void *obj, hpx_parcel_t *p) {
  void *target;
  if (!hpx_gas_try_pin(p->target, &target)) {
    log_action("pinned action resend.\n");
    return HPX_RESEND;
  }

  const action_t *action = obj;
  void *args = hpx_parcel_get_data(p);
  int nargs = *(int*)args;
  size_t *sizes = (size_t*)((char*)args + sizeof(int));
  void *argsp[nargs];
  void *vargs = (char*)sizes + (nargs * sizeof(size_t));
  argsp[0] = (char*)vargs + ALIGN(vargs-args, 8);

  for (int i = 0, e = nargs - 1; i < e; ++i) {
    argsp[i + 1] = (char*)argsp[i] + sizes[i] + ALIGN(sizes[i], 8);
  }

  hpx_pinned_vectored_action_handler_t handler =
      (hpx_pinned_vectored_action_handler_t)action->handler;
  return handler(target, nargs, argsp, sizes);
}

static int _exec_vectored(const void *obj, hpx_parcel_t *p) {
  const action_t *action = obj;
  void *args = hpx_parcel_get_data(p);
  int nargs = *(int*)args;
  size_t *sizes = (size_t*)((char*)args + sizeof(int));
  void *argsp[nargs];
  void *vargs = (char*)sizes + (nargs * sizeof(size_t));
  argsp[0] = (char*)vargs + ALIGN(vargs-args, 8);

  for (int i = 0, e = nargs - 1; i < e; ++i) {
    argsp[i + 1] = (char*)argsp[i] + sizes[i] + ALIGN(sizes[i], 8);
  }

  hpx_vectored_action_handler_t handler =
      (hpx_vectored_action_handler_t )action->handler;
  return handler(nargs, argsp, sizes);
}

void action_init_vectored(action_t *action) {
  uint32_t pinned = action->attr & HPX_PINNED;
  action->exec = (pinned) ? _exec_pinned_vectored : _exec_vectored;
  action->new = _new_vectored;
  action->pack = _pack_vectored;
}
