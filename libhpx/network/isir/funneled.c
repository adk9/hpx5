// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013, Trustees of Indiana University,
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

#include <mpi.h>
#include <stdlib.h>
#include <hpx/builtins.h>
#include <libsync/queues.h>
#include <libhpx/debug.h>
#include <libhpx/libhpx.h>
#include <libhpx/network.h>
#include <libhpx/parcel.h>
#include "irecv_buffer.h"
#include "isend_buffer.h"
#include "isir.h"


#define _CAT1(S, T) S##T
#define _CAT(S, T) _CAT1(S, T)
#define _BYTES(S) (HPX_CACHELINE_SIZE - ((S) % HPX_CACHELINE_SIZE))
#define _PAD(S) const char _CAT(_padding,__LINE__)[_BYTES(S)]


typedef struct {
  network_t                 vtable;
  volatile int               flush;
  _PAD(sizeof(network_t) + sizeof(int));
  two_lock_queue_t sends;
  two_lock_queue_t recvs;
  isend_buffer_t  isends;
  irecv_buffer_t  irecvs;
} _funneled_t;

static const char *_funneled_id() {
  return "Isend/Irecv Funneled";
}

///
static void _send_all(_funneled_t *network) {
  hpx_parcel_t *p = NULL;
  while ((p = sync_two_lock_queue_dequeue(&network->sends))) {
    isend_buffer_append(&network->isends, p, HPX_NULL);
  }
}


/// Delete a funneled network.
static void _funneled_delete(network_t *network) {
  if (!network)
    return;

  _funneled_t *this = (void*)network;

  // flush sends if we're supposed to
  if (this->flush) {
    _send_all(this);
    isend_buffer_flush(&this->isends);
  }

  isend_buffer_fini(&this->isends);
  irecv_buffer_fini(&this->irecvs);

  hpx_parcel_t *p = NULL;
  while ((p = sync_two_lock_queue_dequeue(&this->sends))) {
    hpx_parcel_release(p);
  }
  while ((p = sync_two_lock_queue_dequeue(&this->recvs))) {
    hpx_parcel_release(p);
  }

  sync_two_lock_queue_fini(&this->sends);
  sync_two_lock_queue_fini(&this->recvs);

  free(this);
}


static void _funneled_barrier(network_t *network) {
  MPI_Barrier(MPI_COMM_WORLD);
}


static int _funneled_send(network_t *network, hpx_parcel_t *p,
                          hpx_addr_t l)
{
  _funneled_t *this = (void*)network;
  sync_two_lock_queue_enqueue(&this->sends, p);
  return LIBHPX_OK;
}


static int _funneled_pwc(network_t *network,
                         hpx_addr_t to, void *from, size_t n,
                         hpx_addr_t local, hpx_addr_t remote, hpx_action_t op)
{
  return LIBHPX_EUNIMPLEMENTED;
}


static int _funneled_put(network_t *network,
                         hpx_addr_t to, void *from, size_t n,
                         hpx_addr_t local, hpx_addr_t remote)
{
  return LIBHPX_EUNIMPLEMENTED;
}


static int _funneled_get(network_t *network,
                         void *to, hpx_addr_t from, size_t n,
                         hpx_addr_t local)
{
  return LIBHPX_EUNIMPLEMENTED;
}


static hpx_parcel_t *_funneled_probe(network_t *network, int nrx) {
  _funneled_t *this = (void*)network;
  return sync_two_lock_queue_dequeue(&this->recvs);
}


static void _funneled_set_flush(network_t *network) {
  _funneled_t *this = (void*)network;
  sync_store(&this->flush, 1, SYNC_RELEASE);
}


static int _funneled_progress(network_t *network) {
  _funneled_t *this = (void*)network;
  hpx_parcel_t *chain = irecv_buffer_progress(&this->irecvs);
  int n = 0;
  if (chain) {
    ++n;
    sync_two_lock_queue_enqueue(&this->recvs, chain);
  }

  DEBUG_IF(n) {
    dbg_log_net("completed %d recvs\n", n);
  }

  int m = isend_buffer_progress(&this->isends);

  DEBUG_IF(m) {
    dbg_log_net("completed %d sends\n", m);
  }

  _send_all(this);

  return LIBHPX_OK;

  // suppress unused warnings
  (void)n;
}

network_t *network_isir_funneled_new(struct gas_class *gas, int nrx) {
  _funneled_t *network = malloc(sizeof(*network));
  if (!network) {
    dbg_error("could not allocate a funneled Isend/Irecv network\n");
    return NULL;
  }

  network->vtable.id = _funneled_id;
  network->vtable.delete = _funneled_delete;
  network->vtable.progress = _funneled_progress;
  network->vtable.barrier = _funneled_barrier;
  network->vtable.send = _funneled_send;
  network->vtable.pwc = _funneled_pwc;
  network->vtable.put = _funneled_put;
  network->vtable.get = _funneled_get;
  network->vtable.probe = _funneled_probe;
  network->vtable.set_flush = _funneled_set_flush;

  sync_store(&network->flush, 0, SYNC_RELEASE);
  sync_two_lock_queue_init(&network->sends, NULL);
  sync_two_lock_queue_init(&network->recvs, NULL);
  isend_buffer_init(&network->isends, 64, 0);
  irecv_buffer_init(&network->irecvs, 64, 0);

  return &network->vtable;
}
