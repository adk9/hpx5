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

#include "PWCNetwork.h"
#include "commands.h"
#include "pwc.h"
#include "xport.h"
#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libhpx/gpa.h>
#include <libhpx/locality.h>
#include <libhpx/parcel.h>

using namespace libhpx::network::pwc;

inline void
Command::lcoSet(unsigned src) const
{
  hpx_addr_t lco = offset_to_gpa(here->rank, arg_);
  hpx_lco_set(lco, 0, nullptr, HPX_NULL, HPX_NULL);
}

inline void
Command::lcoSetAtSource(unsigned src) const
{
  dbg_check( pwc_cmd(&PWCNetwork::Impl(), src, Command(), Command(LCO_SET, arg_)) );
}

inline void
Command::deleteParcel(unsigned src) const
{
  auto p = reinterpret_cast<hpx_parcel_t*>(arg_);
  log_net("releasing sent parcel %p\n", static_cast<void*>(p));
  hpx_parcel_t *ssync = p->next;
  p->next = nullptr;
  parcel_delete(p);
  if (ssync) {
    parcel_launch(ssync);
  }
}

inline void
Command::resumeParcel(unsigned src) const
{
  auto p = reinterpret_cast<hpx_parcel_t*>(arg_);
  log_net("resuming %s, (%p)\n", actions[p->action].key, static_cast<void*>(p));
  parcel_launch(p);
}

inline void
Command::resumeParcelAtSource(unsigned src) const
{
  dbg_check( pwc_cmd(&PWCNetwork::Impl(), src, Command(), Command(RESUME_PARCEL, arg_)) );
}

void
Command::operator()(unsigned src) const
{
  switch (op_) {
   case NOP: abort();
   case RESUME_PARCEL:
    resumeParcel(src);
    return;
   case RESUME_PARCEL_SOURCE:
    resumeParcelAtSource(src);
    return;
   case DELETE_PARCEL:
    deleteParcel(src);
    return;
   case LCO_SET:
    lcoSet(src);
    return;
   case LCO_SET_SOURCE:
    lcoSetAtSource(src);
    return;
   case RECV_PARCEL:
    recvParcel(src);
    return;
   case RENDEZVOUS_LAUNCH:
    rendezvousLaunch(src);
    return;
   case RELOAD_REQUEST:
    reloadRequest(src);
    return;
   case RELOAD_REPLY:
    reloadReply(src);
    return;
  }
  unreachable();
}
