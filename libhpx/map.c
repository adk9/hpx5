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
