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

#include "ReloadParcelEmulator.h"
#include "libhpx/debug.h"
#include "libhpx/libhpx.h"

namespace {
using libhpx::network::pwc::PhotonTransport;
using libhpx::network::pwc::Peer;
using libhpx::network::pwc::ReloadParcelEmulator;
}

int
ReloadParcelEmulator::send(unsigned rank, const hpx_parcel_t *p)
{
  ends_[rank].send(p);
  return LIBHPX_OK;
}

ReloadParcelEmulator::~ReloadParcelEmulator()
{
}

ReloadParcelEmulator::ReloadParcelEmulator(const config_t *cfg, boot_t *boot,
                                           gas_t* gas)
    : ranks_(boot_n_ranks(boot)),
      eagerSize_(cfg->pwc_parcelbuffersize),
      ends_(new Peer[ranks_]())
{
  struct Exchange {
    Remote<Peer> peer;
    Remote<char> heap;
  } local;

  local.peer.addr = &ends_[0];
  local.peer.key = PhotonTransport::FindKey(&ends_[0], ranks_ * sizeof(Peer));

  if (gas->type == HPX_GAS_PGAS) {
    size_t n = gas_local_size(gas);
    local.heap.addr = static_cast<char*>(gas_local_base(gas));
    PhotonTransport::Pin(local.heap.addr, n, &local.heap.key);
  }

  std::unique_ptr<Exchange[]> remotes(new Exchange[ranks_]);
  boot_allgather(boot, &local, &remotes[0], sizeof(Exchange));

  unsigned rank = boot_rank(boot);
  for (int i = 0, e = ranks_; i < e; ++i) {
    ends_[i].init(i, rank, remotes[i].peer, remotes[i].heap);
  }
}

void
ReloadParcelEmulator::reload(unsigned src, size_t n)
{
  ends_[src].reload(n, eagerSize_);
}
