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
#include "xport.h"
#include "parcel_utils.h"
#include "libsync/queues.h"
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

typedef struct {
  Network         vtable;
  isir_xport_t    *xport;
  PAD_TO_CACHELINE(sizeof(vtable) + sizeof(xport));
  two_lock_queue_t sends;
  two_lock_queue_t recvs;
  ISendBuffer *isends;
  IRecvBuffer *irecvs;
  Transport *thetransport;
  PAD_TO_CACHELINE(sizeof(sends) + sizeof(recvs) +
                   sizeof(isends) + sizeof(irecvs) + sizeof(thetransport));
  volatile int progress_lock;
} _funneled_t;
}

/// Transfer any parcels in the funneled sends queue into the isends buffer.
static void
_send_all(_funneled_t *network) {
  hpx_parcel_t *p = NULL;
  while ((p = (hpx_parcel_t *)sync_two_lock_queue_dequeue(&network->sends))) {
    hpx_parcel_t *ssync = p->next;
    p->next = NULL;
    network->isends->append(p, ssync);
  }
}

/// Deallocate a funneled network.
static void
_funneled_deallocate(void *network) {
  dbg_assert(network != nullptr);

  _funneled_t *isir = (_funneled_t*)network;
  delete isir->isends;
  delete isir->irecvs;

  hpx_parcel_t *p = NULL;
  while ((p = (hpx_parcel_t *)sync_two_lock_queue_dequeue(&isir->sends))) {
    parcel_delete(p);
  }
  while ((p = (hpx_parcel_t *)sync_two_lock_queue_dequeue(&isir->recvs))) {
    parcel_delete(p);
  }

  sync_two_lock_queue_fini(&isir->sends);
  sync_two_lock_queue_fini(&isir->recvs);

  delete isir->thetransport;

  isir->xport->deallocate(isir->xport);
  free(isir);
}

static int
_funneled_coll_init(void *network, void **ctx)
{
  coll_t *c = *(coll_t **)ctx;
  _funneled_t *isir = (_funneled_t*)network;
  int num_active = c->group_sz;

  log_net("ISIR network collective being initialized."
          "Total active ranks: %d\n", num_active);
  int32_t *ranks = (int32_t*)c->data;

  if (c->comm_bytes == 0) {
    // we have not yet allocated a communicator
    int32_t comm_bytes = isir->xport->sizeof_comm();
    *ctx = realloc(c, sizeof(coll_t) + c->group_bytes + comm_bytes);
    c = *(coll_t**)ctx;
    c->comm_bytes = comm_bytes;
  }

  // setup communicator
  char *comm = c->data + c->group_bytes;

  isir->vtable.flush(network);
  while (!sync_swap(&isir->progress_lock, 0, SYNC_ACQUIRE))
    ;
  isir->xport->create_comm(isir, comm, ranks, num_active, here->ranks);

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
  _funneled_t *isir = (_funneled_t *)network;

  // flushing network is necessary (sufficient?) to execute any
  // packets destined for collective operation
  isir->vtable.flush(network);

  while (!sync_swap(&isir->progress_lock, 0, SYNC_ACQUIRE))
    ;
  if (c->type == ALL_REDUCE) {
    isir->xport->allreduce(sendbuf, out, count, NULL, &c->op, comm);
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

  _funneled_t *isir = (_funneled_t *)network;
  sync_two_lock_queue_enqueue(&isir->sends, p);
  return LIBHPX_OK;
}

static hpx_parcel_t *
_funneled_probe(void *network, int nrx) {
  _funneled_t *isir = (_funneled_t *)network;
  return (hpx_parcel_t *)sync_two_lock_queue_dequeue(&isir->recvs);
}

static void
_funneled_flush(void *network) {
  _funneled_t *isir = (_funneled_t *)network;
  while (!sync_swap(&isir->progress_lock, 0, SYNC_ACQUIRE)) {
  }
  _send_all(isir);
  hpx_parcel_t *ssync = NULL;
  isir->isends->flush(&ssync);
  if (ssync) {
    sync_two_lock_queue_enqueue(&isir->recvs, ssync);
  }
  sync_store(&isir->progress_lock, 1, SYNC_RELEASE);
}

/// Create a network registration.
static void
_funneled_register_dma(void *obj, const void *base, size_t n, void *key) {
  _funneled_t *isir = (_funneled_t *)obj;
  isir->xport->pin(base, n, key);
}

/// Release a network registration.
static void
_funneled_release_dma(void *obj, const void* base, size_t n) {
  _funneled_t *isir = (_funneled_t *)obj;
  isir->xport->unpin(base, n);
}

static int
_funneled_progress(void *network, int id) {
  _funneled_t *isir = (_funneled_t *)network;
  if (sync_swap(&isir->progress_lock, 0, SYNC_ACQUIRE)) {
    hpx_parcel_t *chain = NULL;
    int n = isir->irecvs->progress(&chain);
    DEBUG_IF(n) {
      log_net("completed %d recvs\n", n);
    }
    if (chain) {
      sync_two_lock_queue_enqueue(&isir->recvs, chain);
    }

    chain = NULL;
    n = isir->isends->progress(&chain);
    DEBUG_IF(n) {
      log_net("completed %d sends\n", n);
    }
    if (chain) {
      sync_two_lock_queue_enqueue(&isir->recvs, chain);
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
  _funneled_t *network = nullptr;
  int e = posix_memalign((void**)&network, HPX_CACHELINE_SIZE, sizeof(*network));
  dbg_check(e, "failed to allocate the pwc network structure\n");
  dbg_assert(network);

  network->thetransport = new Transport();

  network->xport = isir_xport_new(cfg, gas, network->thetransport->comm());
  if (!network->xport) {
    log_error("could not initialize a transport.\n");
    free(network);
    return NULL;
  }

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

  sync_two_lock_queue_init(&network->sends, NULL);
  sync_two_lock_queue_init(&network->recvs, NULL);

  network->isends = new ISendBuffer(gas, *network->thetransport, cfg->isir_sendlimit, cfg->isir_testwindow);
  network->irecvs = new IRecvBuffer(*network->thetransport, cfg->isir_recvlimit);

  sync_store(&network->progress_lock, 1, SYNC_RELEASE);

  return &network->vtable;
}
