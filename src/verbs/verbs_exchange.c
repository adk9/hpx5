#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "mpi.h"
#include "libphoton.h"
#include "logging.h"
#include "verbs_exchange.h"
#include "verbs_buffer.h"

extern int _photon_myrank;
extern int _photon_nproc;
extern int _photon_forwarder;
extern verbs_buffer_t *shared_storage;

int __verbs_exchange_ri_ledgers(ProcessInfo *verbs_processes) {
	int i;
	MPI_Request *req;
	MPI_Comm _photon_comm = __photon_config->comm;
	uintptr_t *va;

	ctr_info(" > verbs_exchange_ri_ledgers()");

	va = (uintptr_t *)malloc( _photon_nproc*sizeof(uintptr_t) );
	req = (MPI_Request *)malloc( 2*_photon_nproc*sizeof(MPI_Request) );
	if( !va || !req ) {
		log_err("verbs_exchange_ri_ledgers(): Cannot malloc temporary message buffers\n");
		return -1;
	}
	memset(va, 0, _photon_nproc*sizeof(uintptr_t));
	memset(req, 0, 2*_photon_nproc*sizeof(MPI_Request));

	// Prepare to receive the receive-info ledger rkey and pointers.  The rkey is also used for the send-info ledgers.
	for(i = 0; i < _photon_nproc; i++) {

		if( MPI_Irecv(&(verbs_processes[i].remote_rcv_info_ledger->remote.rkey), sizeof(uint32_t), MPI_BYTE, i, 0, _photon_comm, &req[2*i]) != MPI_SUCCESS ) {
			log_err("verbs_exchange_ri_ledgers(): Couldn't post irecv() for receive-info ledger from task %d", i);
			return -1;
		}

		if( MPI_Irecv(&(va[i]), sizeof(uintptr_t), MPI_BYTE, i, 0, _photon_comm, &req[2*i+1]) != MPI_SUCCESS) {
			log_err("verbs_exchange_ri_ledgers(): Couldn't post irecv() for receive-info ledger from task %d", i);
			return -1;
		}
	}

	// Send the receive-info ledger rkey and pointers
	for(i = 0; i < _photon_nproc; i++) {
		uintptr_t tmp_va;

		if( MPI_Send(&shared_storage->mr->rkey, sizeof(uint32_t), MPI_BYTE, i, 0, _photon_comm) != MPI_SUCCESS) {
			log_err("verbs_exchange_ri_ledgers(): Couldn't send receive-info ledger to process %d", i);
			return -1;
		}

		tmp_va = (uintptr_t)(verbs_processes[i].local_rcv_info_ledger->entries);

		dbg_info("Transmitting rcv_info ledger info to %d: %"PRIxPTR, i, tmp_va);

		if( MPI_Send(&(tmp_va), sizeof(uintptr_t), MPI_BYTE, i, 0, _photon_comm) != MPI_SUCCESS) {
			log_err("verbs_exchange_ri_ledgers(): Couldn't send receive-info ledger to process %d", i);
			return -1;
		}
	}

	// Wait for the arrival of the receive-info ledger rkey and pointers.  The rkey is also used for the send-info ledgers.
	if (MPI_Waitall(2*_photon_nproc, req, MPI_STATUSES_IGNORE) != MPI_SUCCESS) {
		log_err("verbs_exchange_ri_ledgers(): Couldn't wait() for receive-info ledger from task %d", i);
		return -1;
	}
	for(i = 0; i < _photon_nproc; i++) {
		// snd_info and rcv_info ledgers are all stored in the same contiguous memory region and share a common "rkey"
		verbs_processes[i].remote_snd_info_ledger->remote.rkey = verbs_processes[i].remote_rcv_info_ledger->remote.rkey;
		verbs_processes[i].remote_rcv_info_ledger->remote.addr = va[i];
	}


	// Clean up the temp arrays before we reuse them, just to be tidy.  This is not the fast path so we can afford it.
	memset(va, 0, _photon_nproc*sizeof(uintptr_t));
	memset(req, 0, _photon_nproc*sizeof(MPI_Request));
	////////////////////////////////////////////////////////////////////////////////////
	// Prepare to receive the send-info ledger pointers
	for(i = 0; i < _photon_nproc; i++) {
		if( MPI_Irecv(&(va[i]), sizeof(uintptr_t), MPI_BYTE, i, 0, _photon_comm, &req[i]) != MPI_SUCCESS) {
			log_err("verbs_exchange_ri_ledgers(): Couldn't receive send-info ledger from task %d", i);
			return -1;
		}
	}

	// Send the send-info ledger pointers
	for(i = 0; i < _photon_nproc; i++) {
		uintptr_t tmp_va;

		tmp_va = (uintptr_t)(verbs_processes[i].local_snd_info_ledger->entries);

		dbg_info("Transmitting snd_info ledger info to %d: %"PRIxPTR, i, tmp_va);

		if( MPI_Send(&(tmp_va), sizeof(uintptr_t), MPI_BYTE, i, 0, _photon_comm) != MPI_SUCCESS) {
			log_err("verbs_exchange_ri_ledgers(): Couldn't send send-info ledger to task %d", i);
			return -1;
		}
	}

	// Wait for the arrival of the send-info ledger pointers
	if (MPI_Waitall(_photon_nproc, req, MPI_STATUSES_IGNORE) != MPI_SUCCESS) {
		log_err("verbs_exchange_ri_ledgers(): Couldn't wait to receive send-info ledger from task %d", i);
		return -1;
	}
	for(i = 0; i < _photon_nproc; i++) {
		verbs_processes[i].remote_snd_info_ledger->remote.addr = va[i];
	}

	free(va);
	free(req);

	return 0;
}

int __verbs_setup_ri_ledgers(ProcessInfo *verbs_processes, char *buf, int num_entries) {
	int i;
	int ledger_size, offset;

	ctr_info(" > verbs_setup_ri_ledgers()");

	ledger_size = sizeof(verbs_ri_ledger_entry_t) * num_entries;

	// Allocate the receive info ledgers
	for(i = 0; i < _photon_nproc + _photon_forwarder; i++) {
		dbg_info("allocating rcv info ledger for %d: %p", i, (buf + ledger_size * i));
		dbg_info("Offset: %d", ledger_size * i);

		// allocate the ledger
		verbs_processes[i].local_rcv_info_ledger = verbs_ri_ledger_create_reuse((verbs_ri_ledger_entry_t * ) (buf + ledger_size * i), num_entries);
		if (!verbs_processes[i].local_rcv_info_ledger) {
			log_err("verbs_setup_ri_ledgers(): couldn't create local rcv info ledger for process %d", i);
			return -1;
		}

		dbg_info("allocating remote ri ledger for %d: %p", i, buf + ledger_size * (_photon_nproc+_photon_forwarder) + ledger_size * i);
		dbg_info("Offset: %d", ledger_size * _photon_nproc + ledger_size * i);

		verbs_processes[i].remote_rcv_info_ledger = verbs_ri_ledger_create_reuse((verbs_ri_ledger_entry_t * ) (buf + ledger_size * (_photon_nproc+_photon_forwarder) + ledger_size * i), num_entries);
		if (!verbs_processes[i].remote_rcv_info_ledger) {
			log_err("verbs_setup_ri_ledgers(): couldn't create remote rcv info ledger for process %d", i);
			return -1;
		}
	}

	// Allocate the send info ledgers
	offset = 2 * ledger_size * (_photon_nproc+_photon_forwarder);
	for(i = 0; i < _photon_nproc+_photon_forwarder; i++) {
		dbg_info("allocating snd info ledger for %d: %p", i, (buf + offset + ledger_size * i));
		dbg_info("Offset: %d", offset + ledger_size * i);

		// allocate the ledger
		verbs_processes[i].local_snd_info_ledger = verbs_ri_ledger_create_reuse((verbs_ri_ledger_entry_t * ) (buf + offset + ledger_size * i), num_entries);
		if (!verbs_processes[i].local_snd_info_ledger) {
			log_err("verbs_setup_ri_ledgers(): couldn't create local snd info ledger for process %d", i);
			return -1;
		}

		dbg_info("allocating remote ri ledger for %d: %p", i, buf + offset + ledger_size * (_photon_nproc+_photon_forwarder) + ledger_size * i);
		dbg_info("Offset: %d", offset + ledger_size * (_photon_nproc+_photon_forwarder) + ledger_size * i);

		verbs_processes[i].remote_snd_info_ledger = verbs_ri_ledger_create_reuse((verbs_ri_ledger_entry_t * ) (buf + offset + ledger_size * (_photon_nproc+_photon_forwarder) + ledger_size * i), num_entries);
		if (!verbs_processes[i].remote_snd_info_ledger) {
			log_err("verbs_setup_ri_ledgers(): couldn't create remote snd info ledger for process %d", i);
			return -1;
		}
	}

	return 0;
}

int __verbs_exchange_FIN_ledger(ProcessInfo *verbs_processes) {
	int i;
	uintptr_t   *va;
	MPI_Request *req;
	MPI_Comm _photon_comm = __photon_config->comm;

	ctr_info(" > verbs_exchange_FIN_ledger()");

	va = (uintptr_t *)malloc( _photon_nproc*sizeof(uintptr_t) );
	req = (MPI_Request *)malloc( 2*_photon_nproc*sizeof(MPI_Request) );
	if( !va || !req ) {
		log_err("verbs_exchange_FIN_ledgers(): Cannot malloc temporary message buffers\n");
		return -1;
	}
	memset(va, 0, _photon_nproc*sizeof(uintptr_t));
	memset(req, 0, 2*_photon_nproc*sizeof(MPI_Request));

	for(i = 0; i < _photon_nproc; i++) {
		if( MPI_Irecv(&verbs_processes[i].remote_FIN_ledger->remote.rkey, sizeof(uint32_t), MPI_BYTE, i, 0, _photon_comm, &(req[2*i])) != MPI_SUCCESS) {
			log_err("verbs_exchange_FIN_ledger(): Couldn't send rdma info ledger to process %d", i);
			return -1;
		}

		if( MPI_Irecv(&(va[i]), sizeof(uintptr_t), MPI_BYTE, i, 0, _photon_comm, &(req[2*i+1])) != MPI_SUCCESS) {
			log_err("verbs_exchange_FIN_ledger(): Couldn't send rdma info ledger to process %d", i);
			return -1;
		}
	}

	for(i = 0; i < _photon_nproc; i++) {
		uintptr_t tmp_va;

		if( MPI_Send(&shared_storage->mr->rkey, sizeof(uint32_t), MPI_BYTE, i, 0, _photon_comm) != MPI_SUCCESS) {
			log_err("verbs_exchange_FIN_ledger(): Couldn't send rdma send ledger to process %d", i);
			return -1;
		}

		tmp_va = (uintptr_t)(verbs_processes[i].local_FIN_ledger->entries);

		if( MPI_Send(&(tmp_va), sizeof(uintptr_t), MPI_BYTE, i, 0, _photon_comm) != MPI_SUCCESS) {
			log_err("verbs_exchange_FIN_ledger(): Couldn't send rdma info ledger to process %d", i);
			return -1;
		}
	}

	if (MPI_Waitall(2*_photon_nproc,req, MPI_STATUSES_IGNORE) != MPI_SUCCESS) {
		log_err("verbs_exchange_FIN_ledger(): Couldn't send rdma info ledger to process %d", i);
		return -1;
	}
	for(i = 0; i < _photon_nproc; i++) {
		verbs_processes[i].remote_FIN_ledger->remote.addr = va[i];
	}

	return 0;
}

int __verbs_setup_FIN_ledger(ProcessInfo *verbs_processes, char *buf, int num_entries) {
	int i;
	int ledger_size;

	ctr_info(" > verbs_setup_FIN_ledger()");

	ledger_size = sizeof(verbs_rdma_FIN_ledger_entry_t) * num_entries;

	for(i = 0; i < (_photon_nproc+_photon_forwarder); i++) {
		// allocate the ledger
		dbg_info("allocating local FIN ledger for %d", i);

		verbs_processes[i].local_FIN_ledger = verbs_rdma_FIN_ledger_create_reuse((verbs_rdma_FIN_ledger_entry_t *) (buf + ledger_size * i), num_entries);
		if (!verbs_processes[i].local_FIN_ledger) {
			log_err("verbs_setup_FIN_ledger(): couldn't create local FIN ledger for process %d", i);
			return -1;
		}

		dbg_info("allocating remote FIN ledger for %d", i);

		verbs_processes[i].remote_FIN_ledger = verbs_rdma_FIN_ledger_create_reuse((verbs_rdma_FIN_ledger_entry_t *) (buf + ledger_size * (_photon_nproc+_photon_forwarder) + ledger_size * i), num_entries);
		if (!verbs_processes[i].remote_FIN_ledger) {
			log_err("verbs_setup_FIN_ledger(): couldn't create remote FIN ledger for process %d", i);
			return -1;
		}
	}

	return 0;
}

