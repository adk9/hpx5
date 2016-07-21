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
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/scheduler.h>

using namespace libhpx::network::pwc;

/// The fully asynchronous memget operation.
///
/// This isn't being used at the moment, so we have not yet implemented it.
/// @{
int
libhpx::network::pwc::pwc_memget(void *obj, void *to, hpx_addr_t from,
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
    else if (gpa_to_rank(rsync) == gpa_to_rank(from)) {
      rcmd = Command::SetLCO(rsync);
    }
    else {
      hpx_parcel_t *p = action_new_parcel(hpx_lco_set_action, rsync, 0, 0, 0);
      rcmd = Command::ResumeParcelAtSource(p);
    }
  }

  return pwc_get(pwc_network, to, from, size, lcmd, rcmd);
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
namespace {
struct _pwc_memget_rsync_env_t {
  void         *to;
  hpx_addr_t  from;
  size_t         n;
  hpx_addr_t lsync;
};
}

static void
_pwc_memget_rsync_continuation(hpx_parcel_t *p, void *env)
{
  auto e = static_cast<_pwc_memget_rsync_env_t*>(env);
  auto lcmd = Command::Nop();
  auto rcmd = Command::ResumeParcelAtSource(p);

  if (e->lsync) {
    if (gpa_to_rank(e->lsync) == here->rank) {
      lcmd = Command::SetLCO(e->lsync);
    }
    else {
      hpx_parcel_t *c = action_new_parcel(hpx_lco_set_action, e->lsync, 0, 0, 0);
      lcmd = Command::ResumeParcel(c);
    }
  }

  dbg_check( pwc_get(pwc_network, e->to, e->from, e->n, lcmd, rcmd) );
}

int
libhpx::network::pwc::pwc_memget_rsync(void *obj, void *to, hpx_addr_t from,
                                       size_t n, hpx_addr_t lsync)
{
  _pwc_memget_rsync_env_t env = {
    .to     = to,
    .from  = from,
    .n     = n,
    .lsync = lsync
  };
  scheduler_suspend(_pwc_memget_rsync_continuation, &env);
  return HPX_SUCCESS;
}
/// @}

/// The synchronous memget operation.
///
/// This doesn't return until the memget operation has completed.
/// @{
namespace {
struct _pwc_memget_lsync_env_t {
  void        *to;
  hpx_addr_t from;
  size_t        n;
};
}

static void
_pwc_memget_lsync_continuation(hpx_parcel_t *p, void *env)
{
  auto e = static_cast<_pwc_memget_lsync_env_t*>(env);
  auto lcmd = Command::ResumeParcel(p);
  auto rcmd = Command();
  dbg_check( pwc_get(pwc_network, e->to, e->from, e->n, lcmd, rcmd) );
}

int
libhpx::network::pwc::pwc_memget_lsync(void *obj, void *to, hpx_addr_t from,
                                       size_t size)
{
  _pwc_memget_lsync_env_t env = {
    .to = to,
    .from = from,
    .n = size
  };
  scheduler_suspend(_pwc_memget_lsync_continuation, &env);
  return HPX_SUCCESS;
}
/// @}
