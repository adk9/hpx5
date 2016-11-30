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

#include "FunneledNetwork.h"
#include "libhpx/libhpx.h"
#include "libhpx/collective.h"
#include "libhpx/debug.h"
#include "libhpx/parcel.h"
#include "libhpx/action.h"

namespace {
using libhpx::Network;
using libhpx::network::ParcelStringOps;
using libhpx::network::isir::FunneledNetwork;
}

FunneledNetwork::FunneledNetwork(const config_t *cfg, GAS *gas)
    : Network(),
      ParcelStringOps(),
      sends_(),
      recvs_(),
      colls_(),
      xport_(),
      isends_(cfg, gas, xport_),
      irecvs_(cfg, xport_),
      lock_()
{
}

FunneledNetwork::~FunneledNetwork()
{
  while (hpx_parcel_t *p = sends_.dequeue()) {
    parcel_delete(p);
  }
  while (hpx_parcel_t *p = recvs_.dequeue()) {
    parcel_delete(p);
  }
}

int
FunneledNetwork::type() const {
  return HPX_NETWORK_ISIR;
}

void
FunneledNetwork::sendAll() {
  while (hpx_parcel_t *p = sends_.dequeue()) {
    hpx_parcel_t *ssync = p->next;
    p->next = NULL;
    isends_.append((void*)p, ssync, DIRECT);
  }

  while (coll_data_t *p = colls_.dequeue()) {
    hpx_parcel_t *ssync = p->ssync;
    if(p->type == COLL_ALLRED){
      isends_.append((void*)p, ssync, COLL_ALLRED);
    } else{
      //currently only ALLREDUCE is supported
      log_net("ISIR network doesn't support this collective type :%d"
          , COLL_ALLRED);
    }
  }
}

int
FunneledNetwork::coll_init(coll_t **ctx)
{
  flush();

  auto coll = static_cast<coll_t*>(*ctx);
  int num_active = coll->group_sz;
  log_net("ISIR network collective being initialized."
          "Total active ranks: %d\n", num_active);
  int32_t *ranks = reinterpret_cast<int32_t*>(coll->data);

  if (coll->comm_bytes == 0) {
    // we have not yet allocated a communicator
    coll->comm_bytes = sizeof(Transport::Communicator);
    auto bytes = sizeof(coll_t) + coll->group_bytes + coll->comm_bytes;
    coll = static_cast<coll_t*>(realloc(coll, bytes));
    *ctx = coll;
  }

  // setup communicator
  auto offset = coll->data + coll->group_bytes;
  auto comm = reinterpret_cast<Transport::Communicator*>(offset);
  std::lock_guard<std::mutex> _(lock_);
  xport_.createComm(comm, num_active, ranks);
  return LIBHPX_OK;
}

int
FunneledNetwork::coll_sync(coll_data_t *dt, coll_t* c)
{
  hpx_addr_t _and = hpx_lco_and_new(1);
  coll_async(dt, c, _and, HPX_NULL );
  hpx_lco_wait(_and);
  hpx_lco_delete_sync(_and);
  return LIBHPX_OK;
}

int
FunneledNetwork::coll_async(coll_data_t *dt, coll_t* c, hpx_addr_t lsync, hpx_addr_t rsync)
{
  //char *comm = c->data + c->group_bytes;
  auto offset = c->data + c->group_bytes;
  auto comm = reinterpret_cast<Transport::Communicator*>(offset);
  dt->comm = comm;
  dt->type = c->type;
  hpx_parcel_t *ssync_local = action_new_parcel(hpx_lco_set_action, lsync, 0, 0, 0);
  dt->ssync = ssync_local;
  colls_.enqueue(dt);
  return LIBHPX_OK;
}

void
FunneledNetwork::deallocate(const hpx_parcel_t* p)
{
  dbg_error("ISIR network has not network-managed parcels\n");
}

int
FunneledNetwork::send(hpx_parcel_t *p, hpx_parcel_t *ssync) {
  // Use the unused parcel-next pointer to get the ssync continuation parcels
  // through the concurrent queue, along with the primary parcel.
  p->next = ssync;
  sends_.enqueue(p);
  return 0;
}

hpx_parcel_t *
FunneledNetwork::probe(int) {
  return recvs_.dequeue();
}

void
FunneledNetwork::flush()
{
  std::lock_guard<std::mutex> _(lock_);
  sendAll();
  hpx_parcel_t *ssync = NULL;
  isends_.flush(&ssync);
  if (ssync) {
    recvs_.enqueue(ssync);
  }
}

void
FunneledNetwork::pin(const void *base, size_t n, void *key)
{
  xport_.pin(base, n, key);
}

void
FunneledNetwork::unpin(const void* base, size_t n)
{
  xport_.unpin(base, n);
}

void
FunneledNetwork::progress(int)
{
  if (auto _ = std::unique_lock<std::mutex>(lock_, std::try_to_lock)) {
    hpx_parcel_t *chain = NULL;
    if (int n = irecvs_.progress(&chain)) {
      log_net("completed %d recvs\n", n);
      recvs_.enqueue(chain);
    }
    chain = NULL;
    if (int n = isends_.progress(&chain)) {
      log_net("completed %d sends\n", n);
      recvs_.enqueue(chain);
    }
    sendAll();
  }
}
