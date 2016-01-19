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

#include <libhpx/gpa.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/scheduler.h>
#include "pwc.h"

/// The fully asynchronous memget operation.
///
/// This isn't being used at the moment, so we have not yet implemented it.
/// @{
int pwc_memget(void *obj, void *to, hpx_addr_t from, size_t size,
               hpx_addr_t lsync, hpx_addr_t rsync) {
  // need pwc_gwc
  return LIBHPX_EUNIMPLEMENTED;
}
/// @}

/// The remote-synchronous memget operation.
///
/// This is a bit of a strange beast. This will not return until the remote
/// operation has completed, but that's a bit weird since that means that a
/// message has returned from the remote locality. This is probably not
/// different than just using the fully lsync version---except that there is
/// potentially some local rdma going on in that case.
///
/// We currently don't have an interface for get-with-remote-completion, so
/// we're not actually handling this correctly.
/// @{
int pwc_memget_rsync(void *obj, void *to, hpx_addr_t from, size_t n,
                     hpx_addr_t lsync) {
  command_t lcmd = {
    .op  = NOP,
    .arg = lsync
  };

  command_t rcmd = { 0 };

  if (lsync) {
    if (gpa_to_rank(lsync) == here->rank) {
      lcmd.op = LCO_SET;
    }
    else {
      hpx_parcel_t *c = action_new_parcel(hpx_lco_set_action, lsync, 0, 0, 0);
      lcmd.arg = (uintptr_t)c;
      lcmd.op  = RESUME_PARCEL;
    }
  }

  return pwc_get(obj, to, from, n, lcmd, rcmd);
}
/// @}

/// The synchronous memget operation.
///
/// This doesn't return until the memget operation has completed.
typedef struct {
  void   *network;
  void        *to;
  hpx_addr_t from;
  size_t        n;
} _pwc_memget_lsync_env_t;

static void _pwc_memget_lsync_continuation(hpx_parcel_t *p, void *env) {
  _pwc_memget_lsync_env_t *e = env;
  command_t lcmd = {
    .op  = RESUME_PARCEL,
    .arg = (uintptr_t)p
  };
  command_t rcmd = { 0 };
  dbg_check( pwc_get(e->network, e->to, e->from, e->n, lcmd, rcmd) );
}

int pwc_memget_lsync(void *obj, void *to, hpx_addr_t from, size_t size) {
  _pwc_memget_lsync_env_t env = {
    .network = obj,
    .to = to,
    .from = from,
    .n = size
  };
  scheduler_suspend(_pwc_memget_lsync_continuation, &env, 0);
  return HPX_SUCCESS;
}
/// @}
