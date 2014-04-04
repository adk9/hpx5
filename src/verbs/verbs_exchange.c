#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "mpi.h"
#include "libphoton.h"
#include "logging.h"
#include "verbs_exchange.h"
#include "verbs_buffer.h"

extern photonBI shared_storage;

int __verbs_sync_qpn(verbs_cnct_ctx *ctx) {
  unsigned *qp_numbers = (unsigned *)calloc(_photon_nproc, sizeof(unsigned));
  unsigned local_qpn;
  unsigned max_qpn = 0;
  struct ibv_qp *tmp_qp;
  int status, i;

  struct ibv_qp_init_attr attr = {
    .send_cq        = ctx->ib_cq,
    .recv_cq        = ctx->ib_cq,
    .cap            = {
      .max_send_wr     = 1,
      .max_send_sge    = 1,
      .max_recv_wr     = 1,
      .max_recv_sge    = 1,
    },
    .qp_type        = IBV_QPT_UD
  };
  
  tmp_qp = ibv_create_qp(ctx->ib_pd, &attr);
  if (!tmp_qp) {
    dbg_err("Could not create temp QP");
    return PHOTON_ERROR;
  }
  
  local_qpn = tmp_qp->qp_num;

  status = ibv_destroy_qp(tmp_qp);
  if (status) {
    dbg_err("Could not destroy temp QP: %s", strerror(status));
  }

  status = MPI_Allgather(&local_qpn, 1, MPI_UNSIGNED, qp_numbers, 1, MPI_UNSIGNED, MPI_COMM_WORLD);
  if (status != MPI_SUCCESS) {
    dbg_err("Could not allgather QP numbers");
    return PHOTON_ERROR;
  }
  
  for (i = 0; i < _photon_nproc; i++) {
    if (qp_numbers[i] > max_qpn) {
      max_qpn = qp_numbers[i];
    }
  }
  
  dbg_info("my qpn: %u, max qpn: %u", local_qpn, max_qpn);
  //printf("%d: my_qpn: %u, max_qpn: %u\n", _photon_myrank, local_qpn, max_qpn);

  if (local_qpn != max_qpn) {
    for (i = local_qpn; i <= max_qpn; i++) {
      tmp_qp = ibv_create_qp(ctx->ib_pd, &attr);
      local_qpn = tmp_qp->qp_num;
      status = ibv_destroy_qp(tmp_qp);
    }
  }

  dbg_info("new qpn: %u", local_qpn);
  //printf("%d: new qpn: %d\n", _photon_myrank, local_qpn);

  //exit(1);
  return PHOTON_OK;
}

int __verbs_exchange_ri_ledgers(ProcessInfo *verbs_processes) {
  int i;
  MPI_Request *req;
  MPI_Comm _photon_comm = __photon_config->comm;
  uintptr_t *va;

  dbg_info();

  va = (uintptr_t *)malloc( _photon_nproc*sizeof(uintptr_t) );
  req = (MPI_Request *)malloc( 2*_photon_nproc*sizeof(MPI_Request) );
  if( !va || !req ) {
    log_err("Cannot malloc temporary message buffers\n");
    return -1;
  }
  memset(va, 0, _photon_nproc*sizeof(uintptr_t));
  memset(req, 0, 2*_photon_nproc*sizeof(MPI_Request));

  // Prepare to receive the receive-info ledger rkey and pointers.  The rkey is also used for the send-info ledgers.
  for(i = 0; i < _photon_nproc; i++) {

    if( MPI_Irecv(&(verbs_processes[i].remote_rcv_info_ledger->remote.priv), sizeof(struct photon_buffer_priv_t),
                  MPI_BYTE, i, 0, _photon_comm, &req[2*i]) != MPI_SUCCESS ) {
      log_err("Couldn't post irecv() for receive-info ledger from task %d", i);
      return -1;
    }

    if( MPI_Irecv(&(va[i]), sizeof(uintptr_t), MPI_BYTE, i, 0, _photon_comm, &req[2*i+1]) != MPI_SUCCESS) {
      log_err("Couldn't post irecv() for receive-info ledger from task %d", i);
      return PHOTON_ERROR;
    }
  }

  // Send the receive-info ledger rkey and pointers
  for(i = 0; i < _photon_nproc; i++) {
    uintptr_t tmp_va;

    if( MPI_Send(&shared_storage->buf.priv, sizeof(struct photon_buffer_priv_t), MPI_BYTE, i, 0, _photon_comm) != MPI_SUCCESS) {
      log_err("Couldn't send receive-info ledger to process %d", i);
      return PHOTON_ERROR;
    }

    tmp_va = (uintptr_t)(verbs_processes[i].local_rcv_info_ledger->entries);

    dbg_info("Transmitting rcv_info ledger info to %d: %"PRIxPTR, i, tmp_va);

    if( MPI_Send(&(tmp_va), sizeof(uintptr_t), MPI_BYTE, i, 0, _photon_comm) != MPI_SUCCESS) {
      log_err("Couldn't send receive-info ledger to process %d", i);
      return PHOTON_ERROR;
    }
  }

  // Wait for the arrival of the receive-info ledger rkey and pointers.  The rkey is also used for the send-info ledgers.
  if (MPI_Waitall(2*_photon_nproc, req, MPI_STATUSES_IGNORE) != MPI_SUCCESS) {
    log_err("Couldn't wait() for receive-info ledger from task %d", i);
    return PHOTON_ERROR;
  }
  for(i = 0; i < _photon_nproc; i++) {
    // snd_info and rcv_info ledgers are all stored in the same contiguous memory region and share a common "rkey"
    verbs_processes[i].remote_snd_info_ledger->remote.priv = verbs_processes[i].remote_rcv_info_ledger->remote.priv;
    verbs_processes[i].remote_rcv_info_ledger->remote.addr = va[i];
  }


  // Clean up the temp arrays before we reuse them, just to be tidy.  This is not the fast path so we can afford it.
  memset(va, 0, _photon_nproc*sizeof(uintptr_t));
  memset(req, 0, _photon_nproc*sizeof(MPI_Request));
  ////////////////////////////////////////////////////////////////////////////////////
  // Prepare to receive the send-info ledger pointers
  for(i = 0; i < _photon_nproc; i++) {
    if( MPI_Irecv(&(va[i]), sizeof(uintptr_t), MPI_BYTE, i, 0, _photon_comm, &req[i]) != MPI_SUCCESS) {
      log_err("Couldn't receive send-info ledger from task %d", i);
      return PHOTON_ERROR;
    }
  }

  // Send the send-info ledger pointers
  for(i = 0; i < _photon_nproc; i++) {
    uintptr_t tmp_va;

    tmp_va = (uintptr_t)(verbs_processes[i].local_snd_info_ledger->entries);

    dbg_info("Transmitting snd_info ledger info to %d: %"PRIxPTR, i, tmp_va);

    if( MPI_Send(&(tmp_va), sizeof(uintptr_t), MPI_BYTE, i, 0, _photon_comm) != MPI_SUCCESS) {
      log_err("Couldn't send send-info ledger to task %d", i);
      return PHOTON_ERROR;
    }
  }

  // Wait for the arrival of the send-info ledger pointers
  if (MPI_Waitall(_photon_nproc, req, MPI_STATUSES_IGNORE) != MPI_SUCCESS) {
    log_err("Couldn't wait to receive send-info ledger from task %d", i);
    return PHOTON_ERROR;
  }
  for(i = 0; i < _photon_nproc; i++) {
    verbs_processes[i].remote_snd_info_ledger->remote.addr = va[i];
  }

  free(va);
  free(req);

  return PHOTON_OK;
}

int __verbs_exchange_FIN_ledger(ProcessInfo *verbs_processes) {
  int i;
  uintptr_t   *va;
  MPI_Request *req;
  MPI_Comm _photon_comm = __photon_config->comm;

  dbg_info();

  va = (uintptr_t *)malloc( _photon_nproc*sizeof(uintptr_t) );
  req = (MPI_Request *)malloc( 2*_photon_nproc*sizeof(MPI_Request) );
  if( !va || !req ) {
    log_err("Cannot malloc temporary message buffers\n");
    return PHOTON_ERROR;
  }
  memset(va, 0, _photon_nproc*sizeof(uintptr_t));
  memset(req, 0, 2*_photon_nproc*sizeof(MPI_Request));

  for(i = 0; i < _photon_nproc; i++) {
    if( MPI_Irecv(&verbs_processes[i].remote_FIN_ledger->remote.priv, sizeof(struct photon_buffer_priv_t),
                  MPI_BYTE, i, 0, _photon_comm, &(req[2*i])) != MPI_SUCCESS) {
      log_err("Couldn't send rdma info ledger to process %d", i);
      return PHOTON_ERROR;
    }

    if( MPI_Irecv(&(va[i]), sizeof(uintptr_t), MPI_BYTE, i, 0, _photon_comm, &(req[2*i+1])) != MPI_SUCCESS) {
      log_err("Couldn't send rdma info ledger to process %d", i);
      return PHOTON_ERROR;
    }
  }

  for(i = 0; i < _photon_nproc; i++) {
    uintptr_t tmp_va;

    if( MPI_Send(&shared_storage->buf.priv, sizeof(struct photon_buffer_priv_t), MPI_BYTE, i, 0, _photon_comm) != MPI_SUCCESS) {
      log_err("Couldn't send rdma send ledger to process %d", i);
      return PHOTON_ERROR;
    }

    tmp_va = (uintptr_t)(verbs_processes[i].local_FIN_ledger->entries);

    if( MPI_Send(&(tmp_va), sizeof(uintptr_t), MPI_BYTE, i, 0, _photon_comm) != MPI_SUCCESS) {
      log_err("verbs_exchange_FIN_ledger(): Couldn't send rdma info ledger to process %d", i);
      return PHOTON_ERROR;
    }
  }

  if (MPI_Waitall(2*_photon_nproc,req, MPI_STATUSES_IGNORE) != MPI_SUCCESS) {
    log_err("Couldn't send rdma info ledger to process %d", i);
    return PHOTON_ERROR;
  }
  for(i = 0; i < _photon_nproc; i++) {
    verbs_processes[i].remote_FIN_ledger->remote.addr = va[i];
  }

  return PHOTON_OK;
}
