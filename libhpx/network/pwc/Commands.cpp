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
#include "Commands.h"
#include "ReloadParcelEmulator.h"
#include "libhpx/action.h"
#include "libhpx/debug.h"
#include "libhpx/events.h"
#include "libhpx/gpa.h"
#include "libhpx/locality.h"
#include "libhpx/parcel.h"

namespace {
using libhpx::network::pwc::Command;
using libhpx::network::pwc::PWCNetwork;
}

inline void
Command::lcoSet(unsigned src) const
{
  hpx_addr_t lco = offset_to_gpa(here->rank, arg_);
  hpx_lco_set(lco, 0, nullptr, HPX_NULL, HPX_NULL);
}

inline void
Command::lcoSetAtSource(unsigned src) const
{
  PWCNetwork::Cmd(src, Command(), Command(LCO_SET, arg_));
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
  PWCNetwork::Cmd(src, Command(), Command(RESUME_PARCEL, arg_));
}

inline void
Command::reloadReply(unsigned src) const
{
  PWCNetwork::ProgressSends(src);
}

inline void
Command::recvParcel(unsigned src) const
{
#ifdef __LP64__
  auto p = reinterpret_cast<hpx_parcel_t*>(arg_);
#else
  dbg_assert((arg_ & 0xffffffff) == arg_);
  auto p = reinterpret_cast<hpx_parcel_t*>((uintptr_t)arg_);
#endif
  p->src = src;
  parcel_set_state(p, PARCEL_SERIALIZED | PARCEL_BLOCK_ALLOCATED);
  EVENT_PARCEL_RECV(p->id, p->action, p->size, p->src, p->target);
  parcel_launch(p);
}

inline void
Command::reloadRequest(unsigned src) const {
  PWCNetwork::Instance().parcels_.reload(src, arg_);
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
