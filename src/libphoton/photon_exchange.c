#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "libphoton.h"
#include "photon_exchange.h"
#include "logging.h"

extern photonBI shared_storage;

int photon_exchange_ri_ledgers(ProcessInfo *processes) {
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

    if( MPI_Irecv(&(processes[i].remote_rcv_info_ledger->remote.priv), sizeof(struct photon_buffer_priv_t),
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

    tmp_va = (uintptr_t)(processes[i].local_rcv_info_ledger->entries);

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
    processes[i].remote_snd_info_ledger->remote.priv = processes[i].remote_rcv_info_ledger->remote.priv;
    processes[i].remote_rcv_info_ledger->remote.addr = va[i];
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

    tmp_va = (uintptr_t)(processes[i].local_snd_info_ledger->entries);

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
    processes[i].remote_snd_info_ledger->remote.addr = va[i];
  }

  free(va);
  free(req);

  return PHOTON_OK;
}

int photon_exchange_FIN_ledger(ProcessInfo *processes) {
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
    if( MPI_Irecv(&processes[i].remote_FIN_ledger->remote.priv, sizeof(struct photon_buffer_priv_t),
                  MPI_BYTE, i, 0, _photon_comm, &(req[2*i])) != MPI_SUCCESS) {
      log_err("Couldn't recv rdma info ledger for process %d", i);
      return PHOTON_ERROR;
    }

    if( MPI_Irecv(&(va[i]), sizeof(uintptr_t), MPI_BYTE, i, 0, _photon_comm, &(req[2*i+1])) != MPI_SUCCESS) {
      log_err("Couldn't recv rdma info ledger for process %d", i);
      return PHOTON_ERROR;
    }
  }

  for(i = 0; i < _photon_nproc; i++) {
    uintptr_t tmp_va;

    if( MPI_Send(&shared_storage->buf.priv, sizeof(struct photon_buffer_priv_t), MPI_BYTE, i, 0, _photon_comm) != MPI_SUCCESS) {
      log_err("Couldn't send rdma send ledger to process %d", i);
      return PHOTON_ERROR;
    }

    tmp_va = (uintptr_t)(processes[i].local_FIN_ledger->entries);

    if( MPI_Send(&(tmp_va), sizeof(uintptr_t), MPI_BYTE, i, 0, _photon_comm) != MPI_SUCCESS) {
      log_err("Couldn't send rdma info ledger to process %d", i);
      return PHOTON_ERROR;
    }
  }

  if (MPI_Waitall(2*_photon_nproc,req, MPI_STATUSES_IGNORE) != MPI_SUCCESS) {
    log_err("Couldn't send rdma info ledger to process %d", i);
    return PHOTON_ERROR;
  }
  for(i = 0; i < _photon_nproc; i++) {
    processes[i].remote_FIN_ledger->remote.addr = va[i];
  }

  return PHOTON_OK;
}

int photon_exchange_eager_buf(ProcessInfo *processes) {
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
    if( MPI_Irecv(&processes[i].eager_buf->remote.priv, sizeof(struct photon_buffer_priv_t),
                  MPI_BYTE, i, 0, _photon_comm, &(req[2*i])) != MPI_SUCCESS) {
      log_err("Couldn't recv rdma eager buf for process %d", i);
      return PHOTON_ERROR;
    }

    if( MPI_Irecv(&(va[i]), sizeof(uintptr_t), MPI_BYTE, i, 0, _photon_comm, &(req[2*i+1])) != MPI_SUCCESS) {
      log_err("Couldn't recv rdma eager buf for process %d", i);
      return PHOTON_ERROR;
    }
  }

  for(i = 0; i < _photon_nproc; i++) {
    uintptr_t tmp_va;

    if( MPI_Send(&shared_storage->buf.priv, sizeof(struct photon_buffer_priv_t), MPI_BYTE, i, 0, _photon_comm) != MPI_SUCCESS) {
      log_err("Couldn't send rdma eager buf to process %d", i);
      return PHOTON_ERROR;
    }

    tmp_va = (uintptr_t)(processes[i].eager_buf->entries);

    if( MPI_Send(&(tmp_va), sizeof(uintptr_t), MPI_BYTE, i, 0, _photon_comm) != MPI_SUCCESS) {
      log_err("Couldn't send rdma eager buf to process %d", i);
      return PHOTON_ERROR;
    }
  }

  if (MPI_Waitall(2*_photon_nproc, req, MPI_STATUSES_IGNORE) != MPI_SUCCESS) {
    log_err("Couldn't send rdma eager buf to process %d", i);
    return PHOTON_ERROR;
  }
  for(i = 0; i < _photon_nproc; i++) {
    processes[i].eager_buf->remote.addr = va[i];
  }

  return PHOTON_OK;
}

int photon_setup_ri_ledgers(ProcessInfo *photon_processes, char *buf, int num_entries) {
  int i;
  int ledger_size, offset;

  dbg_info();

  ledger_size = sizeof(struct photon_ri_ledger_entry_t) * num_entries;

  // Allocate the receive info ledgers
  for(i = 0; i < (_photon_nproc + _photon_nforw); i++) {
    dbg_info("allocating rcv info ledger for %d: %p", i, (buf + ledger_size * i));
    dbg_info("Offset: %d", ledger_size * i);

    // allocate the ledger
    photon_processes[i].local_rcv_info_ledger = photon_ri_ledger_create_reuse((photonRILedgerEntry) (buf + ledger_size * i), num_entries);
    if (!photon_processes[i].local_rcv_info_ledger) {
      log_err("couldn't create local rcv info ledger for process %d", i);
      return PHOTON_ERROR;
    }

    dbg_info("allocating remote ri ledger for %d: %p", i, buf + ledger_size * (_photon_nproc + _photon_nforw) + ledger_size * i);
    dbg_info("Offset: %d", ledger_size * (_photon_nproc + _photon_nforw) + ledger_size * i);

    photon_processes[i].remote_rcv_info_ledger = photon_ri_ledger_create_reuse((photonRILedgerEntry) (buf + ledger_size * (_photon_nproc + _photon_nforw) + ledger_size * i), num_entries);
    if (!photon_processes[i].remote_rcv_info_ledger) {
      log_err("couldn't create remote rcv info ledger for process %d", i);
      return PHOTON_ERROR;
    }
  }

  // Allocate the send info ledgers
  offset = 2 * ledger_size * (_photon_nproc + _photon_nforw);
  for(i = 0; i < (_photon_nproc + _photon_nforw); i++) {
    dbg_info("allocating snd info ledger for %d: %p", i, (buf + offset + ledger_size * i));
    dbg_info("Offset: %d", offset + ledger_size * i);

    // allocate the ledger
    photon_processes[i].local_snd_info_ledger = photon_ri_ledger_create_reuse((photonRILedgerEntry) (buf + offset + ledger_size * i), num_entries);
    if (!photon_processes[i].local_snd_info_ledger) {
      log_err("couldn't create local snd info ledger for process %d", i);
      return PHOTON_ERROR;
    }

    dbg_info("allocating remote ri ledger for %d: %p", i, buf + offset + ledger_size * (_photon_nproc + _photon_nforw) + ledger_size * i);
    dbg_info("Offset: %d", offset + ledger_size * (_photon_nproc + _photon_nforw) + ledger_size * i);

    photon_processes[i].remote_snd_info_ledger = photon_ri_ledger_create_reuse((photonRILedgerEntry) (buf + offset + ledger_size * (_photon_nproc + _photon_nforw) + ledger_size * i), num_entries);
    if (!photon_processes[i].remote_snd_info_ledger) {
      log_err("couldn't create remote snd info ledger for process %d", i);
      return PHOTON_ERROR;
    }
  }

  return PHOTON_OK;
}

int photon_setup_FIN_ledger(ProcessInfo *photon_processes, char *buf, int num_entries) {
  int i;
  int ledger_size;

  dbg_info();

  ledger_size = sizeof(struct photon_rdma_FIN_ledger_entry_t) * num_entries;

  for(i = 0; i < (_photon_nproc + _photon_nforw); i++) {
    // allocate the ledger
    dbg_info("allocating local FIN ledger for %d", i);

    photon_processes[i].local_FIN_ledger = photon_rdma_FIN_ledger_create_reuse((photonFINLedgerEntry) (buf + ledger_size * i), num_entries);
    if (!photon_processes[i].local_FIN_ledger) {
      log_err("couldn't create local FIN ledger for process %d", i);
      return PHOTON_ERROR;
    }

    dbg_info("allocating remote FIN ledger for %d", i);

    photon_processes[i].remote_FIN_ledger = photon_rdma_FIN_ledger_create_reuse((photonFINLedgerEntry) (buf + ledger_size * (_photon_nproc + _photon_nforw) + ledger_size * i), num_entries);
    if (!photon_processes[i].remote_FIN_ledger) {
      log_err("couldn't create remote FIN ledger for process %d", i);
      return PHOTON_ERROR;
    }
  }

  return PHOTON_OK;
}

int photon_setup_EAGER_buf(ProcessInfo *photon_processes, char *buf, int num_entries) {
  int i;
  int buf_size;

  dbg_info();

  buf_size = sizeof(struct photon_rdma_eager_buf_entry_t) * num_entries;

  for(i = 0; i < (_photon_nproc + _photon_nforw); i++) {
    
    dbg_info("allocating eager buffer for %d", i);
    
    photon_processes[i].eager_buf = photon_rdma_eager_buf_create_reuse((photonEagerBufEntry) (buf + buf_size * i), num_entries);
    if (!photon_processes[i].eager_buf) {
      log_err("couldn't create eager buffer for process %d", i);
      return PHOTON_ERROR;
    }
  } 

  return PHOTON_OK;
}
