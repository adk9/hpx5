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

/// @file libhpx/call.c
/// @brief Implement the hpx/call.h header.
///

#include <string.h>
#include <stdarg.h>
#include <hpx/hpx.h>
#include <libhpx/action.h>
#include <libhpx/parcel.h>
#include <libhpx/scheduler.h>

/// A RPC call with a user-specified continuation action.
int _hpx_call_with_continuation(hpx_addr_t addr, hpx_action_t action,
                                hpx_addr_t c_target, hpx_action_t c_action,
                                int nargs, ...) {
  va_list vargs;
  va_start(vargs, nargs);
  int e = action_call_va(addr, action, c_target, c_action, HPX_NULL, HPX_NULL,
                         nargs, &vargs);
  va_end(vargs);
  return e;
}

/// Encapsulates an asynchronous remote-procedure-call.
int _hpx_call(hpx_addr_t addr, hpx_action_t action, hpx_addr_t result,
              int nargs, ...) {
  va_list vargs;
  va_start(vargs, nargs);
  int e = action_call_va(addr, action, result, hpx_lco_set_action, HPX_NULL,
                         HPX_NULL, nargs, &vargs);
  va_end(vargs);
  return e;
}

int _hpx_call_sync(hpx_addr_t addr, hpx_action_t action, void *out,
                   size_t olen, int nargs, ...) {
  hpx_addr_t result = hpx_lco_future_new(olen);
  va_list vargs;
  va_start(vargs, nargs);
  int e = action_call_va(addr, action, result, hpx_lco_set_action, HPX_NULL,
                         HPX_NULL, nargs, &vargs);
  va_end(vargs);

  if (e == HPX_SUCCESS) {
    e = hpx_lco_get(result, olen, out);
  }

  hpx_lco_delete(result, HPX_NULL);
  return e;
}

int _hpx_call_when(hpx_addr_t gate, hpx_addr_t addr, hpx_action_t action,
                   hpx_addr_t result, int nargs, ...) {
  va_list vargs;
  va_start(vargs, nargs);
  int e = action_call_va(addr, action, result, hpx_lco_set_action, HPX_NULL,
                         gate, nargs, &vargs);
  va_end(vargs);
  return e;
}

int _hpx_call_when_sync(hpx_addr_t gate, hpx_addr_t addr, hpx_action_t action,
                        void *out, size_t olen, int nargs, ...) {
  hpx_addr_t result = hpx_lco_future_new(olen);
  va_list vargs;
  va_start(vargs, nargs);
  int e = action_call_va(addr, action, result, hpx_lco_set_action, HPX_NULL,
                         gate, nargs, &vargs);
  va_end(vargs);

  if (e == HPX_SUCCESS) {
    e = hpx_lco_get(result, olen, out);
  }

  hpx_lco_delete(result, HPX_NULL);
  return e;
}

/// hpx_call_when with a user-specified continuation action.
int _hpx_call_when_with_continuation(hpx_addr_t gate, hpx_addr_t addr,
                                     hpx_action_t action, hpx_addr_t c_target,
                                     hpx_action_t c_action, int nargs, ...) {
  va_list vargs;
  va_start(vargs, nargs);
  int e = action_call_va(addr, action, c_target, c_action, HPX_NULL, gate,
                         nargs, &vargs);
  va_end(vargs);
  return e;
}

int _hpx_call_async(hpx_addr_t addr, hpx_action_t action,
                    hpx_addr_t lsync, hpx_addr_t result, int nargs, ...) {
  va_list vargs;
  va_start(vargs, nargs);
  int e = action_call_va(addr, action, result, hpx_lco_set_action, lsync,
                         HPX_NULL, nargs, &vargs);
  va_end(vargs);
  return e;
}

void _hpx_call_when_cc(hpx_addr_t gate, hpx_addr_t addr, hpx_action_t action,
                      void (*cleanup)(void*), void *env, int nargs, ...) {
  hpx_parcel_t *p = scheduler_current_parcel();
  va_list vargs;
  va_start(vargs, nargs);
  int e = action_call_va(addr, action, p->c_target, p->c_action, HPX_NULL, gate,
                         nargs, &vargs);
  va_end(vargs);
  if (e == HPX_SUCCESS) {
    p->c_target = HPX_NULL;
    p->c_action = HPX_NULL;
    hpx_thread_continue_cleanup(cleanup, env, NULL, 0);
  }

  hpx_thread_exit(e);
}
