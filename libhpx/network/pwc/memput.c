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
#include <libhpx/scheduler.h>
#include "commands.h"
#include "pwc.h"

/// The asynchronous memput operation.
///
/// We;re going to satisfy this operation using pwc, but that only lets us deal
/// with lsync and rsync LCO addresses at either the destination or the
/// source. If the LCO is at a third location we'll allocate a local parcel to
/// encode that, and use a resume command to launch it for the appropriate
/// event. This requires an extra hop for the rsync operation.
///
/// If we wanted to be fancy we could use a parcel if the size of the data is
/// small. This would trade off the extra hop for a bit of extra bandwidth.
/// @{
int pwc_memput(void *obj, hpx_addr_t to, const void *from, size_t size,
               hpx_addr_t lsync, hpx_addr_t rsync) {
  hpx_action_t lcmd = 0;
  if (lsync) {
    if (gpa_to_rank(lsync) == here->rank) {
      lcmd = lco_set;
    }
    else {
      hpx_parcel_t *l = action_new_parcel(hpx_lco_set_action, lsync, 0, 0, 0);
      lsync = (uint64_t)l;
      lcmd = resume_parcel;
    }
  }

  hpx_action_t rcmd = 0;
  if (rsync) {
    if (gpa_to_rank(rsync) == here->rank) {
      rcmd = lco_set_source;
    }
    else if (gpa_to_rank(rsync) == gpa_to_rank(to)) {
      rcmd = lco_set;
    }
    else {
      hpx_parcel_t *r = action_new_parcel(hpx_lco_set_action, rsync, 0, 0, 0);
      rsync = (uint64_t)r;
      rcmd = resume_parcel_source;
    }
  }

  return pwc_pwc(obj, to, from, size, lcmd, lsync, rcmd, rsync);
}
/// @}

/// The lsync memput implementation.
///
/// This can't return until the local buffer can be rewritten. We're using
/// put/pwc under the hood though, which requires a local command. We'll do a
/// simple resume continuation locally to handle this.
///
/// This operation is complicated by the fact that the remote sync LCO is set by
/// the user, and may be anywhere in the system. The pwc interface gives us the
/// ability to set LCOs at the source or the destination relatively directly,
/// but if it's in a third place we need to do something special about it.
///
/// We will allocate a local parcel to store the continuation and attach an
/// rsync resume operation, which requires an extra hop for the continuation but
/// is the easiest option. If we wanted to branch here we could use a parcel if
/// the put is small.
/// @{
typedef struct {
  void        *obj;
  hpx_addr_t    to;
  const void *from;
  size_t         n;
  hpx_addr_t rsync;
} _pwc_memput_lsync_continuation_env_t;

static void _pwc_memput_lsync_continuation(hpx_parcel_t *p, void *env) {
  _pwc_memput_lsync_continuation_env_t *e = env;

  hpx_action_t rcmd = 0;
  hpx_addr_t  rsync = e->rsync;
  if (rsync) {
    if (gpa_to_rank(rsync) == here->rank) {
      rcmd = lco_set_source;
    }
    else if (gpa_to_rank(rsync) == gpa_to_rank(e->to)) {
      rcmd = lco_set;
    }
    else {
      hpx_parcel_t *r = action_new_parcel(hpx_lco_set_action, rsync, 0, 0, 0);
      rcmd = resume_parcel_source;
      rsync = (uintptr_t)r;
    }
  }

  hpx_action_t lcmd = resume_parcel;
  hpx_addr_t  lsync = (uintptr_t)p;
  dbg_check( pwc_pwc(e->obj, e->to, e->from, e->n, lcmd, lsync, rcmd, rsync) );
}

int pwc_memput_lsync(void *obj, hpx_addr_t to, const void *from, size_t n,
                     hpx_addr_t rsync) {
  _pwc_memput_lsync_continuation_env_t env = {
    .obj = obj,
    .to = to,
    .from = from,
    .n = n,
    .rsync = rsync
  };
  scheduler_suspend(_pwc_memput_lsync_continuation, &env, 0);
  return HPX_SUCCESS;
}
/// @}

/// The rsync memput operation.
///
/// This shouldn't return until the remote operation has completed. We can just
/// use pwc() to do this, with a remote command that wakes us up.
/// @{
typedef struct {
  void        *obj;
  hpx_addr_t    to;
  const void *from;
  size_t         n;
} _pwc_memput_rsync_continuation_env_t;

static void _pwc_memput_rsync_continuation(hpx_parcel_t *p, void *env) {
  _pwc_memput_rsync_continuation_env_t *e = env;
  hpx_action_t rcmd = resume_parcel_source;
  hpx_addr_t  rsync = (uintptr_t)p;
  dbg_check( pwc_pwc(e->obj, e->to, e->from, e->n, 0, 0, rcmd, rsync) );
}

int pwc_memput_rsync(void *obj, hpx_addr_t to, const void *from, size_t n) {
  _pwc_memput_rsync_continuation_env_t env = {
    .obj = obj,
    .to = to,
    .from = from,
    .n = n
  };
  scheduler_suspend(_pwc_memput_rsync_continuation, &env, 0);
  return HPX_SUCCESS;
}
/// @}
