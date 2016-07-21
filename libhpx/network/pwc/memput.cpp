// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2016, Trustees of Indiana University,
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

#include "pwc.h"
#include <libhpx/gpa.h>
#include <libhpx/locality.h>
#include <libhpx/scheduler.h>

using namespace libhpx::network::pwc;

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
int
libhpx::network::pwc::pwc_memput(void *obj, hpx_addr_t to, const void *from,
                                 size_t size, hpx_addr_t lsync,
                                 hpx_addr_t rsync)
{
  auto lcmd = Command::Nop();
  auto rcmd = Command::Nop();

  if (lsync) {
    if (gpa_to_rank(lsync) == here->rank) {
      lcmd = Command::SetLCO(lsync);
    }
    else {
      hpx_parcel_t *p = action_new_parcel(hpx_lco_set_action, lsync, 0, 0, 0);
      dbg_assert(p);
      lcmd = Command::ResumeParcel(p);
    }
  }

  if (rsync) {
    if (gpa_to_rank(rsync) == here->rank) {
      rcmd = Command::SetLCOAtSource(rsync);
    }
    else if (gpa_to_rank(rsync) == gpa_to_rank(to)) {
      rcmd = Command::SetLCO(rsync);
    }
    else {
      hpx_parcel_t *p = action_new_parcel(hpx_lco_set_action, rsync, 0, 0, 0);
      rcmd = Command::ResumeParcelAtSource(p);
    }
  }

  return pwc_put(pwc_network, to, from, size, lcmd, rcmd);
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
namespace {
struct _pwc_memput_lsync_continuation_env_t {
  hpx_addr_t    to;
  const void *from;
  size_t         n;
  hpx_addr_t rsync;
};
}

static void
_pwc_memput_lsync_continuation(hpx_parcel_t *p, void *env)
{
  auto e = static_cast<_pwc_memput_lsync_continuation_env_t*>(env);
  auto rcmd = Command::Nop();

  if (e->rsync) {
    if (gpa_to_rank(e->rsync) == here->rank) {
      rcmd = Command::SetLCOAtSource(e->rsync);
    }
    else if (gpa_to_rank(e->rsync) == gpa_to_rank(e->to)) {
      rcmd = Command::SetLCO(e->rsync);
    }
    else {
      hpx_parcel_t *p = action_new_parcel(hpx_lco_set_action, e->rsync, 0, 0, 0);
      rcmd = Command::ResumeParcelAtSource(p);
    }
  }

  auto lcmd = Command::ResumeParcel(p);
  dbg_check( pwc_put(pwc_network, e->to, e->from, e->n, lcmd, rcmd) );
}

int
libhpx::network::pwc::pwc_memput_lsync(void *obj, hpx_addr_t to,
                                       const void *from, size_t n,
                                       hpx_addr_t rsync)
{
  _pwc_memput_lsync_continuation_env_t env = {
    .to = to,
    .from = from,
    .n = n,
    .rsync = rsync
  };
  scheduler_suspend(_pwc_memput_lsync_continuation, &env);
  return HPX_SUCCESS;
}
/// @}

/// The rsync memput operation.
///
/// This shouldn't return until the remote operation has completed. We can just
/// use pwc() to do this, with a remote command that wakes us up.
/// @{
namespace {
struct _pwc_memput_rsync_continuation_env_t {
  hpx_addr_t    to;
  const void *from;
  size_t         n;
};
}

static void
_pwc_memput_rsync_continuation(hpx_parcel_t *p, void *env)
{
  auto e = static_cast<_pwc_memput_rsync_continuation_env_t*>(env);
  auto lcmd = Command::Nop();
  auto rcmd = Command::ResumeParcelAtSource(p);
  dbg_check( pwc_put(pwc_network, e->to, e->from, e->n, lcmd, rcmd) );
}

int
libhpx::network::pwc::pwc_memput_rsync(void *obj, hpx_addr_t to,
                                       const void *from, size_t n)
{
  _pwc_memput_rsync_continuation_env_t env = {
    .to = to,
    .from = from,
    .n = n
  };
  scheduler_suspend(_pwc_memput_rsync_continuation, &env);
  return HPX_SUCCESS;
}
/// @}
