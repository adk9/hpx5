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

#include <inttypes.h>
#include <stdlib.h>
#include <hpx/builtins.h>
#include <libsync/queues.h>
#include <unistd.h>

#include <libhpx/debug.h>
#include <libhpx/gas.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/padding.h>
#include <libhpx/parcel.h>
#include <mpi.h>

#include "irecv_buffer.h"
#include "isend_buffer.h"
#include "isir.h"
#include "xport.h"
#include "parcel_utils.h"

typedef struct {
  network_t       vtable;
  gas_t             *gas;
  isir_xport_t    *xport;
  PAD_TO_CACHELINE(sizeof(network_t) + sizeof(gas_t*) + sizeof(isir_xport_t*));
  two_lock_queue_t sends;
  two_lock_queue_t recvs;
  isend_buffer_t  isends;
  irecv_buffer_t  irecvs;
  PAD_TO_CACHELINE(2 * sizeof(two_lock_queue_t) +
                   sizeof(irecv_buffer_t) +
                   sizeof(isend_buffer_t));
  volatile int progress_lock;
} _funneled_t;

/// Transfer any parcels in the funneled sends queue into the isends buffer.
static void
_send_all(_funneled_t *network) {
  hpx_parcel_t *p = NULL;
  while ((p = sync_two_lock_queue_dequeue(&network->sends))) {
    isend_buffer_append(&network->isends, p, HPX_NULL);
  }
}

/// Delete a funneled network.
static void
_funneled_delete(void *network) {
  dbg_assert(network);

  _funneled_t *isir = network;
  isend_buffer_fini(&isir->isends);
  irecv_buffer_fini(&isir->irecvs);

  hpx_parcel_t *p = NULL;
  while ((p = sync_two_lock_queue_dequeue(&isir->sends))) {
    parcel_delete(p);
  }
  while ((p = sync_two_lock_queue_dequeue(&isir->recvs))) {
    parcel_delete(p);
  }

  sync_two_lock_queue_fini(&isir->sends);
  sync_two_lock_queue_fini(&isir->recvs);

  isir->xport->delete(isir->xport);
  free(isir);
}

static int _funneled_coll_init(void *network, coll_t **_c){
  coll_t* c = *_c;
  int num_active = c->group_sz;

  log_net("ISIR network collective being initialized."
		  " Total active ranks : %d \n", num_active);
  int32_t* ranks = (int32_t*) c->data;
  
  if(c->comm_bytes == 0){
    //we have not yet allocated a communicator
    int32_t comm_bytes = sizeof(MPI_Comm);
    *_c = realloc(c, sizeof(coll_t) + c->group_bytes + comm_bytes); 
    c = *_c;
    c->comm_bytes = comm_bytes;
  }

  //setup communicator
  char *comm = c->data + c->group_bytes;

  _funneled_t* isir = network;
  //isir->vtable.flush(network);
  while (!sync_swap(&isir->progress_lock, 0, SYNC_ACQUIRE))
    ;
  isir->xport->create_comm(comm, ranks, num_active, here->ranks);
  
  sync_store(&isir->progress_lock, 1, SYNC_RELEASE);
  return LIBHPX_OK;	
}

static int _funneled_coll_sync(void *network, void *in, size_t input_sz, void* out, coll_t* c){
  void *sendbuf = in;
  int count     = input_sz;
  char *comm = c->data + c->group_bytes;
  _funneled_t* isir = network;
  
  //flushing network is necessary (sufficient ?) to execute any packets
  //destined for collective operation
  //isir->vtable.flush(network);

  MPI_Request req;
  MPI_Comm *communicator = comm;
  MPI_Status status;
  int flag = 0;
  
  while (!sync_swap(&isir->progress_lock, 0, SYNC_ACQUIRE))
   ;
  int ret = MPI_Iallreduce(sendbuf, out, 1, MPI_DOUBLE, MPI_MIN, *communicator, &req);
  if(ret != MPI_SUCCESS){
    printf("error in allreduce\n");
  }  
  sync_store(&isir->progress_lock, 1, SYNC_RELEASE);
  
  while(!flag){ 
  
    if(sync_swap(&isir->progress_lock, 0, SYNC_ACQUIRE)){
  
      MPI_Test(&req, &flag, &status);
      if(c->type == ALL_REDUCE) {
        //isir->xport->allreduce(sendbuf, out, count, NULL, &c->op, comm);
      } else {
        log_dflt("Collective type descriptor : %d is Invalid! \n", c->type);
      }
  
      sync_store(&isir->progress_lock, 1, SYNC_RELEASE);
    }
    if(!flag)
      usleep(400);
  
 }
  return LIBHPX_OK;
}

static int
_funneled_send(void *network, hpx_parcel_t *p) {
  _funneled_t *isir = network;
  sync_two_lock_queue_enqueue(&isir->sends, p);
  return LIBHPX_OK;
}

static hpx_parcel_t *
_funneled_probe(void *network, int nrx) {
  _funneled_t *isir = network;
  return sync_two_lock_queue_dequeue(&isir->recvs);
}

static void
_funneled_flush(void *network) {
  _funneled_t *isir = network;
  while (!sync_swap(&isir->progress_lock, 0, SYNC_ACQUIRE)) {
  }
  _send_all(isir);
  isend_buffer_flush(&isir->isends);
  sync_store(&isir->progress_lock, 1, SYNC_RELEASE);
}

/// Create a network registration.
static void
_funneled_register_dma(void *obj, const void *base, size_t n, void *key) {
  _funneled_t *isir = obj;
  isir->xport->pin(base, n, key);
}

/// Release a network registration.
static void
_funneled_release_dma(void *obj, const void* base, size_t n) {
  _funneled_t *isir = obj;
  isir->xport->unpin(base, n);
}

static int
_funneled_progress(void *network, int id) {
  _funneled_t *isir = network;
  if (sync_swap(&isir->progress_lock, 0, SYNC_ACQUIRE)) {
    hpx_parcel_t *chain = irecv_buffer_progress(&isir->irecvs);
    int n = 0;
    if (chain) {
      ++n;
      sync_two_lock_queue_enqueue(&isir->recvs, chain);
    }

    DEBUG_IF(n) {
      log_net("completed %d recvs\n", n);
    }

    int m = isend_buffer_progress(&isir->isends);

    DEBUG_IF(m) {
      log_net("completed %d sends\n", m);
    }

    _send_all(isir);
    sync_store(&isir->progress_lock, 1, SYNC_RELEASE);
    (void)n;
    (void)m;
  }
  return LIBHPX_OK;

  // suppress unused warnings
}

network_t *
network_isir_funneled_new(const config_t *cfg, struct boot *boot, gas_t *gas) {
  _funneled_t *network = NULL;
  int e = posix_memalign((void*)&network, HPX_CACHELINE_SIZE, sizeof(*network));
  dbg_check(e, "failed to allocate the pwc network structure\n");
  dbg_assert(network);

  network->xport = isir_xport_new(cfg, gas);
  if (!network->xport) {
    log_error("could not initialize a transport.\n");
    free(network);
    return NULL;
  }

  network->vtable.type = HPX_NETWORK_ISIR;
  network->vtable.string = &isir_string_vtable;
  network->vtable.delete = _funneled_delete;
  network->vtable.progress = _funneled_progress;
  network->vtable.send = _funneled_send;
  network->vtable.coll_sync = _funneled_coll_sync;
  network->vtable.coll_init = _funneled_coll_init;
  network->vtable.probe = _funneled_probe;
  network->vtable.flush = _funneled_flush;
  network->vtable.register_dma = _funneled_register_dma;
  network->vtable.release_dma = _funneled_release_dma;
  network->vtable.lco_get = isir_lco_get;
  network->vtable.lco_wait = isir_lco_wait;
  network->gas = gas;

  sync_two_lock_queue_init(&network->sends, NULL);
  sync_two_lock_queue_init(&network->recvs, NULL);

  isend_buffer_init(&network->isends, network->xport, 64, cfg->isir_sendlimit,
            cfg->isir_testwindow);
  irecv_buffer_init(&network->irecvs, network->xport, 64, cfg->isir_recvlimit);

  sync_store(&network->progress_lock, 1, SYNC_RELEASE);

  return &network->vtable;
}
