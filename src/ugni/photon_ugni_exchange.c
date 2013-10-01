#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "libphoton.h"
#include "logging.h"
#include "photon_ugni_exchange.h"
#include "photon_ugni_buffer.h"

extern int _photon_myrank;
extern int _photon_nproc;
extern int _photon_forwarder;
extern photonBuffer shared_storage;

int __ugni_exchange_ri_ledgers(UgniProcessInfo *ugni_processes) {
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

		if( MPI_Irecv(&(ugni_processes[i].remote_rcv_info_ledger->remote.mdh), sizeof(gni_mem_handle_t), MPI_BYTE, i, 0, _photon_comm, &req[2*i]) != MPI_SUCCESS ) {
			log_err("Couldn't post irecv() for receive-info ledger from task %d", i);
			return -1;
		}

		if( MPI_Irecv(&(va[i]), sizeof(uintptr_t), MPI_BYTE, i, 0, _photon_comm, &req[2*i+1]) != MPI_SUCCESS) {
			log_err("Couldn't post irecv() for receive-info ledger from task %d", i);
			return -1;
		}
	}

	// Send the receive-info ledger rkey and pointers
	for(i = 0; i < _photon_nproc; i++) {
		uintptr_t tmp_va;

		if( MPI_Send(&(shared_storage->mdh), sizeof(gni_mem_handle_t), MPI_BYTE, i, 0, _photon_comm) != MPI_SUCCESS) {
			log_err("Couldn't send receive-info ledger to process %d", i);
			return -1;
		}

		tmp_va = (uintptr_t)(ugni_processes[i].local_rcv_info_ledger->entries);

		dbg_info("Transmitting rcv_info ledger info to %d: %"PRIxPTR, i, tmp_va);

		if( MPI_Send(&(tmp_va), sizeof(uintptr_t), MPI_BYTE, i, 0, _photon_comm) != MPI_SUCCESS) {
			log_err("Couldn't send receive-info ledger to process %d", i);
			return -1;
		}
	}

	// Wait for the arrival of the receive-info ledger rkey and pointers.  The rkey is also used for the send-info ledgers.
	if (MPI_Waitall(2*_photon_nproc, req, MPI_STATUSES_IGNORE) != MPI_SUCCESS) {
		log_err("Couldn't wait() for receive-info ledger from task %d", i);
		return -1;
	}
	for(i = 0; i < _photon_nproc; i++) {
		// snd_info and rcv_info ledgers are all stored in the same contiguous memory region and share a common memory handle
		ugni_processes[i].remote_snd_info_ledger->remote.mdh = ugni_processes[i].remote_rcv_info_ledger->remote.mdh;
		ugni_processes[i].remote_rcv_info_ledger->remote.addr = va[i];
	}


	// Clean up the temp arrays before we reuse them, just to be tidy.  This is not the fast path so we can afford it.
	memset(va, 0, _photon_nproc*sizeof(uintptr_t));
	memset(req, 0, _photon_nproc*sizeof(MPI_Request));
	////////////////////////////////////////////////////////////////////////////////////
	// Prepare to receive the send-info ledger pointers
	for(i = 0; i < _photon_nproc; i++) {
		if( MPI_Irecv(&(va[i]), sizeof(uintptr_t), MPI_BYTE, i, 0, _photon_comm, &req[i]) != MPI_SUCCESS) {
			log_err("Couldn't receive send-info ledger from task %d", i);
			return -1;
		}
	}

	// Send the send-info ledger pointers
	for(i = 0; i < _photon_nproc; i++) {
		uintptr_t tmp_va;

		tmp_va = (uintptr_t)(ugni_processes[i].local_snd_info_ledger->entries);

		dbg_info("Transmitting snd_info ledger info to %d: %"PRIxPTR, i, tmp_va);

		if( MPI_Send(&(tmp_va), sizeof(uintptr_t), MPI_BYTE, i, 0, _photon_comm) != MPI_SUCCESS) {
			log_err("Couldn't send send-info ledger to task %d", i);
			return -1;
		}
	}

	// Wait for the arrival of the send-info ledger pointers
	if (MPI_Waitall(_photon_nproc, req, MPI_STATUSES_IGNORE) != MPI_SUCCESS) {
		log_err("Couldn't wait to receive send-info ledger from task %d", i);
		return -1;
	}
	for(i = 0; i < _photon_nproc; i++) {
		ugni_processes[i].remote_snd_info_ledger->remote.addr = va[i];
	}

	free(va);
	free(req);

	return 0;
}

int __ugni_setup_ri_ledgers(UgniProcessInfo *ugni_processes, void *buf, int num_entries) {
	int i;
	int ledger_size, offset;

	dbg_info();

	ledger_size = sizeof(struct photon_ri_ledger_entry_t) * num_entries;

	// Allocate the receive info ledgers
	for(i = 0; i < _photon_nproc + _photon_forwarder; i++) {
		dbg_info("allocating rcv info ledger for %d: %p", i, (buf + ledger_size * i));
		dbg_info("Offset: %d", ledger_size * i);

		// allocate the ledger
		ugni_processes[i].local_rcv_info_ledger = photon_ri_ledger_create_reuse((photonRILedgerEntry) (buf + ledger_size * i), num_entries);
		if (!ugni_processes[i].local_rcv_info_ledger) {
			log_err("couldn't create local rcv info ledger for process %d", i);
			return -1;
		}

		dbg_info("allocating remote ri ledger for %d: %p", i, buf + ledger_size * (_photon_nproc+_photon_forwarder) + ledger_size * i);
		dbg_info("Offset: %d", ledger_size * _photon_nproc + ledger_size * i);

		ugni_processes[i].remote_rcv_info_ledger = photon_ri_ledger_create_reuse((photonRILedgerEntry) (buf + ledger_size * (_photon_nproc+_photon_forwarder) + ledger_size * i), num_entries);
		if (!ugni_processes[i].remote_rcv_info_ledger) {
			log_err("couldn't create remote rcv info ledger for process %d", i);
			return -1;
		}
	}

	// Allocate the send info ledgers
	offset = 2 * ledger_size * (_photon_nproc+_photon_forwarder);
	for(i = 0; i < _photon_nproc+_photon_forwarder; i++) {
		dbg_info("allocating snd info ledger for %d: %p", i, (buf + offset + ledger_size * i));
		dbg_info("Offset: %d", offset + ledger_size * i);

		// allocate the ledger
		ugni_processes[i].local_snd_info_ledger = photon_ri_ledger_create_reuse((photonRILedgerEntry) (buf + offset + ledger_size * i), num_entries);
		if (!ugni_processes[i].local_snd_info_ledger) {
			log_err("couldn't create local snd info ledger for process %d", i);
			return -1;
		}

		dbg_info("allocating remote ri ledger for %d: %p", i, buf + offset + ledger_size * (_photon_nproc+_photon_forwarder) + ledger_size * i);
		dbg_info("Offset: %d", offset + ledger_size * (_photon_nproc+_photon_forwarder) + ledger_size * i);

		ugni_processes[i].remote_snd_info_ledger = photon_ri_ledger_create_reuse((photonRILedgerEntry) (buf + offset + ledger_size * (_photon_nproc+_photon_forwarder) + ledger_size * i), num_entries);
		if (!ugni_processes[i].remote_snd_info_ledger) {
			log_err("couldn't create remote snd info ledger for process %d", i);
			return -1;
		}
	}

	return 0;
}

int __ugni_exchange_FIN_ledger(UgniProcessInfo *ugni_processes) {
	int i;
	uintptr_t   *va;
	MPI_Request *req;
	MPI_Comm _photon_comm = __photon_config->comm;

	dbg_info();

	va = (uintptr_t *)malloc( _photon_nproc*sizeof(uintptr_t) );
	req = (MPI_Request *)malloc( 2*_photon_nproc*sizeof(MPI_Request) );
	if( !va || !req ) {
		log_err("Cannot malloc temporary message buffers\n");
		return -1;
	}
	memset(va, 0, _photon_nproc*sizeof(uintptr_t));
	memset(req, 0, 2*_photon_nproc*sizeof(MPI_Request));

	for(i = 0; i < _photon_nproc; i++) {
		if( MPI_Irecv(&(ugni_processes[i].remote_FIN_ledger->remote.mdh), sizeof(gni_mem_handle_t), MPI_BYTE, i, 0, _photon_comm, &(req[2*i])) != MPI_SUCCESS) {
			log_err("Couldn't send rdma info ledger to process %d", i);
			return -1;
		}

		if( MPI_Irecv(&(va[i]), sizeof(uintptr_t), MPI_BYTE, i, 0, _photon_comm, &(req[2*i+1])) != MPI_SUCCESS) {
			log_err("Couldn't send rdma info ledger to process %d", i);
			return -1;
		}
	}

	for(i = 0; i < _photon_nproc; i++) {
		uintptr_t tmp_va;

		if( MPI_Send(&shared_storage->mdh, sizeof(gni_mem_handle_t), MPI_BYTE, i, 0, _photon_comm) != MPI_SUCCESS) {
			log_err("Couldn't send rdma send ledger to process %d", i);
			return -1;
		}

		tmp_va = (uintptr_t)(ugni_processes[i].local_FIN_ledger->entries);

		if( MPI_Send(&(tmp_va), sizeof(uintptr_t), MPI_BYTE, i, 0, _photon_comm) != MPI_SUCCESS) {
			log_err("Couldn't send rdma info ledger to process %d", i);
			return -1;
		}
	}

	if (MPI_Waitall(2*_photon_nproc,req, MPI_STATUSES_IGNORE) != MPI_SUCCESS) {
		log_err("Couldn't send rdma info ledger to process %d", i);
		return -1;
	}
	for(i = 0; i < _photon_nproc; i++) {
		ugni_processes[i].remote_FIN_ledger->remote.addr = va[i];
	}

	return 0;
}

int __ugni_setup_FIN_ledger(UgniProcessInfo *ugni_processes, void *buf, int num_entries) {
	int i;
	int ledger_size;

	dbg_info();

	ledger_size = sizeof(struct photon_rdma_FIN_ledger_entry_t) * num_entries;

	for(i = 0; i < (_photon_nproc+_photon_forwarder); i++) {
		// allocate the ledger
		dbg_info("allocating local FIN ledger for %d", i);

		ugni_processes[i].local_FIN_ledger = photon_rdma_FIN_ledger_create_reuse((photonFINLedgerEntry) (buf + ledger_size * i), num_entries);
		if (!ugni_processes[i].local_FIN_ledger) {
			log_err("couldn't create local FIN ledger for process %d", i);
			return -1;
		}

		dbg_info("allocating remote FIN ledger for %d", i);

		ugni_processes[i].remote_FIN_ledger = photon_rdma_FIN_ledger_create_reuse((photonFINLedgerEntry) (buf + ledger_size * (_photon_nproc+_photon_forwarder) + ledger_size * i), num_entries);
		if (!ugni_processes[i].remote_FIN_ledger) {
			log_err("couldn't create remote FIN ledger for process %d", i);
			return -1;
		}
	}

	return 0;
}

