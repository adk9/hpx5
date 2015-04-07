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

static int _vabcast(hpx_action_t action, hpx_addr_t lsync, hpx_addr_t rsync,
                    int nargs, va_list *vargs) {
  int e = HPX_SUCCESS;
  hpx_addr_t remote = HPX_NULL;
  hpx_addr_t local = HPX_NULL;
  if (rsync) {
    remote = hpx_lco_and_new(here->ranks);
    e = hpx_call_when_with_continuation(remote, rsync, hpx_lco_set_action,
                                        remote, hpx_lco_delete_action, NULL, 0);
    dbg_check(e, "could not chain LCO\n");
  }

  if (lsync) {
    local = hpx_lco_and_new(here->ranks);
    e = hpx_call_when_with_continuation(local, lsync, hpx_lco_set_action,
                                        local, hpx_lco_delete_action, NULL, 0);
    dbg_check(e, "could not chain LCO\n");
  }

  for (int i = 0, e = here->ranks; i < e; ++i) {
    va_list temp;
    va_copy(temp, *vargs);
    int e = libhpx_call_action(HPX_THERE(i), action, remote, hpx_lco_set_action,
                               local, HPX_NULL, nargs, &temp);
    dbg_check(e, "error generating parcel for bcast.\n");
    va_end(temp);
  }
  return HPX_SUCCESS;
}

int _hpx_bcast(hpx_action_t action, hpx_addr_t lsync, hpx_addr_t rsync,
               int nargs, ...) {
    va_list vargs;
    va_start(vargs, nargs);
    int e = _vabcast(action, lsync, rsync, nargs, &vargs);
    dbg_check(e, "failed broadcast\n");
    va_end(vargs);
    return e;
}

int _hpx_bcast_lsync(hpx_action_t action, hpx_addr_t rsync, int nargs, ...) {
  hpx_addr_t lsync = hpx_lco_future_new(0);
  if (lsync == HPX_NULL) {
    log_error("could not allocate an LCO.\n");
    return HPX_ENOMEM;
  }

  va_list vargs;
  va_start(vargs, nargs);
  if (HPX_SUCCESS != _vabcast(action, lsync, rsync, nargs, &vargs)) {
    dbg_error("failed broadcast\n");
  }
  va_end(vargs);

  if (HPX_SUCCESS != hpx_lco_wait(lsync)) {
    dbg_error("failed broadcast\n");
  }

  hpx_lco_delete(lsync, HPX_NULL);
  return HPX_SUCCESS;
}

int _hpx_bcast_rsync(hpx_action_t action, int nargs, ...) {
  hpx_addr_t rsync = hpx_lco_future_new(0);
  if (rsync == HPX_NULL) {
    log_error("could not allocate an LCO.\n");
    return HPX_ENOMEM;
  }

  va_list vargs;
  va_start(vargs, nargs);
  if (HPX_SUCCESS != _vabcast(action, HPX_NULL, rsync, nargs, &vargs)) {
    dbg_error("failed broadcast\n");
  }
  va_end(vargs);

  if (HPX_SUCCESS != hpx_lco_wait(rsync)) {
    dbg_error("failed broadcast\n");
  }

  hpx_lco_delete(rsync, HPX_NULL);
  return HPX_SUCCESS;
}

