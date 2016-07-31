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

#include "isir.h"
#include "MPITransport.h"
#include "IRecvBuffer.h"
#include "ISendBuffer.h"
#include "libsync/queues.hpp"
#include "libhpx/collective.h"
#include "libhpx/debug.h"
#include "libhpx/libhpx.h"
#include "libhpx/locality.h"
#include "libhpx/padding.h"
#include "libhpx/parcel.h"
#include "hpx/builtins.h"
#include <inttypes.h>
#include <stdlib.h>

namespace {
using Transport = libhpx::network::isir::MPITransport;
using IRecvBuffer = libhpx::network::isir::IRecvBuffer;
using ISendBuffer = libhpx::network::isir::ISendBuffer;
using ParcelQueue = libsync::TwoLockQueue<hpx_parcel_t*>;

struct ISIRNetwork {
  Network         vtable;
  PAD_TO_CACHELINE(sizeof(vtable));
  ParcelQueue*     sends;
  ParcelQueue*     recvs;
  ISendBuffer*    isends;
  IRecvBuffer*    irecvs;
  Transport*       xport;
  PAD_TO_CACHELINE(sizeof(sends) + sizeof(recvs) + sizeof(isends) +
                   sizeof(irecvs) + sizeof(xport));
  volatile int progress_lock;
};
}

/// Transfer any parcels in the funneled sends queue into the isends buffer.
static void
_send_all(ISIRNetwork *network) {
  while (hpx_parcel_t *p = network->sends->dequeue()) {
    hpx_parcel_t *ssync = p->next;
    p->next = NULL;
    network->isends->append(p, ssync);
  }
}

/// Deallocate a funneled network.
static void
_funneled_deallocate(void *network) {
  dbg_assert(network != nullptr);

  ISIRNetwork *isir = (ISIRNetwork*)network;

  while (hpx_parcel_t *p = isir->sends->dequeue()) {
    parcel_delete(p);
  }
  while (hpx_parcel_t *p = isir->recvs->dequeue()) {
    parcel_delete(p);
  }

  delete isir->sends;
  delete isir->recvs;
  delete isir->isends;
  delete isir->irecvs;
  delete isir->xport;

  free(isir);
}

static int
_funneled_coll_init(void *network, void **ctx)
{
  coll_t *c = *(coll_t **)ctx;
  ISIRNetwork *isir = (ISIRNetwork*)network;
  int num_active = c->group_sz;

  log_net("ISIR network collective being initialized."
          "Total active ranks: %d\n", num_active);
  int32_t *ranks = (int32_t*)c->data;

  if (c->comm_bytes == 0) {
    // we have not yet allocated a communicator
    int32_t comm_bytes = sizeof(Transport::Communicator);
    *ctx = realloc(c, sizeof(coll_t) + c->group_bytes + comm_bytes);
    c = *(coll_t**)ctx;
    c->comm_bytes = comm_bytes;
  }

  // setup communicator
  char *comm = c->data + c->group_bytes;

  isir->vtable.flush(network);
  while (!sync_swap(&isir->progress_lock, 0, SYNC_ACQUIRE))
    ;
  isir->xport->createComm(reinterpret_cast<Transport::Communicator*>(comm), num_active, ranks);

  sync_store(&isir->progress_lock, 1, SYNC_RELEASE);
  return LIBHPX_OK;
}

static int
_funneled_coll_sync(void *network, void *in, size_t input_sz, void *out,
                    void *ctx)
{
  coll_t *c = (coll_t *)ctx;
  void *sendbuf = in;
  int count = input_sz;
  char *comm = c->data + c->group_bytes;
  ISIRNetwork *isir = (ISIRNetwork *)network;

  // flushing network is necessary (sufficient?) to execute any
  // packets destined for collective operation
  isir->vtable.flush(network);

  while (!sync_swap(&isir->progress_lock, 0, SYNC_ACQUIRE))
    ;
  if (c->type == ALL_REDUCE) {
    isir->xport->allreduce(sendbuf, out, count, NULL, &c->op,
                                  reinterpret_cast<Transport::Communicator*>(comm));
  } else {
    log_dflt("Collective type descriptor: %d is invalid!\n", c->type);
  }
  sync_store(&isir->progress_lock, 1, SYNC_RELEASE);
  return LIBHPX_OK;
}

static int
_funneled_send(void *network, hpx_parcel_t *p, hpx_parcel_t *ssync) {
  // Use the unused parcel-next pointer to get the ssync continuation parcels
  // through the concurrent queue, along with the primary parcel.
  dbg_assert(p->next == NULL);
  p->next = ssync;

  ISIRNetwork *isir = (ISIRNetwork *)network;
  isir->sends->enqueue(p);
  return LIBHPX_OK;
}

static hpx_parcel_t *
_funneled_probe(void *network, int nrx) {
  ISIRNetwork *isir = (ISIRNetwork *)network;
  return isir->recvs->dequeue();
}

static void
_funneled_flush(void *network) {
  ISIRNetwork *isir = (ISIRNetwork *)network;
  while (!sync_swap(&isir->progress_lock, 0, SYNC_ACQUIRE)) {
  }
  _send_all(isir);
  hpx_parcel_t *ssync = NULL;
  isir->isends->flush(&ssync);
  if (ssync) {
    isir->recvs->enqueue(ssync);
  }
  sync_store(&isir->progress_lock, 1, SYNC_RELEASE);
}

/// Create a network registration.
static void
_funneled_register_dma(void *obj, const void *base, size_t n, void *key) {
  ISIRNetwork *isir = (ISIRNetwork *)obj;
  isir->xport->pin(base, n, key);
}

/// Release a network registration.
static void
_funneled_release_dma(void *obj, const void* base, size_t n) {
  ISIRNetwork *isir = (ISIRNetwork *)obj;
  isir->xport->unpin(base, n);
}

static int
_funneled_progress(void *network, int id) {
  ISIRNetwork *isir = (ISIRNetwork *)network;
  if (sync_swap(&isir->progress_lock, 0, SYNC_ACQUIRE)) {
    hpx_parcel_t *chain = NULL;
    int n = isir->irecvs->progress(&chain);
    DEBUG_IF(n) {
      log_net("completed %d recvs\n", n);
    }
    if (chain) {
      isir->recvs->enqueue(chain);
    }

    chain = NULL;
    n = isir->isends->progress(&chain);
    DEBUG_IF(n) {
      log_net("completed %d sends\n", n);
    }
    if (chain) {
      isir->recvs->enqueue(chain);
    }

    _send_all(isir);
    sync_store(&isir->progress_lock, 1, SYNC_RELEASE);
    (void)n;
  }
  return LIBHPX_OK;

  // suppress unused warnings
}

void *
network_isir_funneled_new(const config_t *cfg, struct boot *boot, struct gas *gas) {
  ISIRNetwork *network = nullptr;
  int e = posix_memalign((void**)&network, HPX_CACHELINE_SIZE, sizeof(*network));
  dbg_check(e, "failed to allocate the pwc network structure\n");
  dbg_assert(network);

  network->vtable.type         = HPX_NETWORK_ISIR;
  network->vtable.string       = &parcel_string_vtable;
  network->vtable.deallocate   = _funneled_deallocate;
  network->vtable.progress     = _funneled_progress;
  network->vtable.send         = _funneled_send;
  network->vtable.coll_sync    = _funneled_coll_sync;
  network->vtable.coll_init    = _funneled_coll_init;
  network->vtable.probe        = _funneled_probe;
  network->vtable.flush        = _funneled_flush;
  network->vtable.register_dma = _funneled_register_dma;
  network->vtable.release_dma  = _funneled_release_dma;
  network->vtable.lco_get      = isir_lco_get;
  network->vtable.lco_wait     = isir_lco_wait;

  network->sends = new ParcelQueue();
  network->recvs = new ParcelQueue();
  network->xport = new Transport();
  network->isends = new ISendBuffer(gas, *network->xport, cfg->isir_sendlimit, cfg->isir_testwindow);
  network->irecvs = new IRecvBuffer(*network->xport, cfg->isir_recvlimit);

  sync_store(&network->progress_lock, 1, SYNC_RELEASE);

  return &network->vtable;
}
