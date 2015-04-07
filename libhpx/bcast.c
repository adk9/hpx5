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
// #include "libhpx/libhpx.h"
#include <libhpx/locality.h>

/// Encapsulates a RPC called on all available localities.
int _hpx_bcast(hpx_action_t action, hpx_addr_t lsync, hpx_addr_t rsync, int
               nargs, ...) {
  hpx_addr_t and = HPX_NULL;
  if (rsync) {
    and = hpx_lco_and_new(here->ranks);
    hpx_call_when_with_continuation(and, rsync, hpx_lco_set_action,
                                    and, hpx_lco_delete_action, NULL, 0);
  }

  for (int i = 0, e = here->ranks; i < e; ++i) {
    va_list vargs;
    va_start(vargs, nargs);
    int e = libhpx_call_action(HPX_THERE(i), action, and, hpx_lco_set_action,
                               HPX_NULL, HPX_NULL, nargs, &vargs);
    dbg_check(e, "hpx_bcast returned an error.\n");
    va_end(vargs);
  }
  return HPX_SUCCESS;
}

int _hpx_bcast_lsync(hpx_action_t action, hpx_addr_t rsync, int nargs, ...) {
  int e;
  hpx_addr_t lco = hpx_lco_future_new(0);
  if (lco == HPX_NULL) {
    e = log_error("could not allocate an LCO.\n");
    goto unwind0;
  }

  hpx_addr_t and = hpx_lco_and_new(here->ranks);
  hpx_call_when_with_continuation(and, lco, hpx_lco_set_action,
                                  and, hpx_lco_delete_action, NULL, 0);

  for (int i = 0, e = here->ranks; i < e; ++i) {
    va_list vargs;
    va_start(vargs, nargs);
    int e = libhpx_call_action(HPX_THERE(i), action, and, hpx_lco_set_action,
                               HPX_NULL, HPX_NULL, nargs, &vargs);
    dbg_check(e, "hpx_bcast returned an error.\n");
    va_end(vargs);
  }

  e = hpx_lco_wait(lco);
  DEBUG_IF(e != HPX_SUCCESS) {
    e = log_error("error waiting for bcast and gate");
    goto unwind1;
  }

 unwind1:
  hpx_lco_delete(lco, HPX_NULL);
 unwind0:
  return e;
}

int _hpx_bcastrlsync(hpx_action_t action, hpx_addr_t rsync, int nargs, ...) {
  int e;
  hpx_addr_t lco = hpx_lco_future_new(0);
  if (lco == HPX_NULL) {
    e = log_error("could not allocate an LCO.\n");
    goto unwind0;
  }

  hpx_addr_t and = hpx_lco_and_new(here->ranks);
  hpx_call_when_with_continuation(and, lco, hpx_lco_set_action,
                                  and, hpx_lco_delete_action, NULL, 0);

  for (int i = 0, e = here->ranks; i < e; ++i) {
    va_list vargs;
    va_start(vargs, nargs);
    int e = libhpx_call_action(HPX_THERE(i), action, and, hpx_lco_set_action,
                               HPX_NULL, HPX_NULL, nargs, &vargs);
    dbg_check(e, "hpx_bcast returned an error.\n");
    va_end(vargs);
  }

  e = hpx_lco_wait(lco);
  DEBUG_IF(e != HPX_SUCCESS) {
    e = log_error("error waiting for bcast and gate");
    goto unwind1;
  }

 unwind1:
  hpx_lco_delete(lco, HPX_NULL);
 unwind0:
  return e;
}

