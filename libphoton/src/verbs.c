#include "mpi.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#include "photon.h"
#include "verbs.h"
#include "verbs_buffer.h"
#include "verbs_remote_buffer.h"
#include "htable.h"
#include "buffertable.h"
#include "logging.h"
#include "counter.h"

#ifdef WITH_XSP

static int verbs_xsp_init();
static int verbs_xsp_setup_session(libxspSess **sess, char *xsp_hop);
static int verbs_xsp_connect_phorwarder();
static int verbs_xsp_exchange_ri_ledgers();
static int verbs_xsp_exchange_FIN_ledger();

static int _photon_fp;
static int sess_count;
static pthread_mutex_t sess_mtx;

#endif

int _photon_nproc;
int _photon_myrank;
int _photon_forwarder;
MPI_Comm _photon_comm;

static struct verbs_cnct_info **exch_cnct_info(int num_qp, struct verbs_cnct_info **local_info);
static int verbs_connect_qps(int num_qp, struct verbs_cnct_info *local_info, struct verbs_cnct_info *remote_info, ProcessInfo *verbs_process);
static int verbs_exchange_ri_ledgers();
static int verbs_setup_ri_ledgers(char *buf, int num_entries);
static int verbs_exchange_FIN_ledger();
static int verbs_setup_FIN_ledger(char *buf, int num_entries);
int        verbs_register_buffer(char *buffer, int buffer_size);
static int verbs_init_context(ProcessInfo *verbs_processes);
static int verbs_connect_peers(ProcessInfo *verbs_processes);

// We only want to spawn a dedicated thread for ledgers on
// multithreaded instantiations of the library (e.g. in xspd).
// FIXME: All of the pthreads stuff below should also depend on this.
#ifdef PHOTON_MULTITHREADED
static pthread_t ledger_watcher;
static void *verbs_req_watcher(void *arg);
#else
static int __verbs_wait_event(verbs_req_t *req);
static int __verbs_wait_ledger(verbs_req_t *req);
static int __verbs_nbpop_ledger(verbs_req_t *req);
static int __verbs_nbpop_event(verbs_req_t *req);
#endif

static inline verbs_req_t *verbs_get_request();
verbs_buffer_t *shared_storage;

static ProcessInfo *verbs_processes;

static char *              phot_verbs_ib_dev = NULL;
static int                 phot_verbs_ib_port = 1;
static struct ibv_context *phot_verbs_context;
static struct ibv_pd      *phot_verbs_pd;
static struct ibv_cq      *phot_verbs_cq;
static struct ibv_srq     *phot_verbs_srq;
static int                 phot_verbs_lid = -1;

static htable_t *reqtable, *ledger_reqtable;
static struct verbs_req *requests;
static int num_requests;
static LIST_HEAD(freereqs, verbs_req) free_reqs_list;
static LIST_HEAD(unreapedevdreqs, verbs_req) unreaped_evd_reqs_list;
static LIST_HEAD(unreapedledgerreqs, verbs_req) unreaped_ledger_reqs_list;
static LIST_HEAD(pendingreqs, verbs_req) pending_reqs_list;
static SLIST_HEAD(pendingmemregs, mem_register_req) pending_mem_register_list;

static int __initialized = 0;
DEFINE_COUNTER(curr_cookie, uint32_t)
DEFINE_COUNTER(handshake_rdma_write, uint32_t)


//////// Global variables ////////
#ifdef DEBUG

#endif

///////////////////////////////////////////////////////////////////////////////
#ifdef DEBUG
int _photon_start_debugging=1;
#endif
#if defined(DEBUG) || defined(CALLTRACE)
FILE *_phot_ofp;

static time_t _tictoc(time_t stime, int proc) {
	time_t etime;
	etime = time(NULL);
	if ((etime - stime) > 10) {
		if( proc >= 0 )
			dbg_info("Still waiting for a recv buffer from %d", proc);
		else
			dbg_info("Still waiting for a recv buffer from any peer");
		stime = etime;
	}
	return stime;
}
#endif

///////////////////////////////////////////////////////////////////////////////
int verbs_init_common(photonConfig cfg) {
	int i;
	char *buf;
	int bufsize, offset;
	int info_ledger_size, FIN_ledger_size;

	_photon_nproc = cfg->nproc;
	_photon_myrank = (int)cfg->address;
	_photon_forwarder = cfg->use_forwarder;
	_photon_comm = cfg->comm;

	if (cfg->ib_dev)
		phot_verbs_ib_dev = cfg->ib_dev;

	if (cfg->ib_port)
		phot_verbs_ib_port = cfg->ib_port;

	if (__initialized != 0) {
		log_err("verbs_init_common(): Error: already initialized/initializing");
		goto error_exit;
	}

	ctr_info(" > verbs_init_common(%d, %d)",_photon_nproc, _photon_myrank);

	// __initialized: 0 - not; -1 - initializing; 1 - initialized
	__initialized = -1;
	INIT_COUNTER(curr_cookie, 1);
	INIT_COUNTER(handshake_rdma_write, 0);

	requests = malloc(sizeof(verbs_req_t) * DEF_NUM_REQUESTS);
	if (!requests) {
		log_err("verbs_init_common(): Failed to allocate request list");
		goto error_exit_req;
	}

	num_requests = DEF_NUM_REQUESTS;
	LIST_INIT(&free_reqs_list);
	LIST_INIT(&unreaped_evd_reqs_list);
	LIST_INIT(&unreaped_ledger_reqs_list);
	LIST_INIT(&pending_reqs_list);

	for(i = 0; i < num_requests; i++) {
		LIST_INSERT_HEAD(&free_reqs_list, &(requests[i]), list);
	}

	dbg_info("create_buffertable()");
	fflush(stderr);
	if (buffertable_init(193)) {
		log_err("verbs_init_common(); Failed to allocate buffer table");
		goto error_exit_req;
	}

	dbg_info("create_reqtable()");
	reqtable = htable_create(193);
	if (!reqtable) {
		log_err("verbs_init_common(): Failed to allocate request table");
		goto error_exit_bt;
	}

	dbg_info("create_ledger_reqtable()");

	ledger_reqtable = htable_create(193);
	if (!ledger_reqtable) {
		log_err("verbs_init_common(): Failed to allocate request table");
		goto error_exit_rt;
	}

	verbs_processes = (ProcessInfo *) malloc(sizeof(ProcessInfo) * (_photon_nproc+_photon_forwarder));
	if (!verbs_processes) {
		log_err("verbs_init_common(): Couldn't allocate process information");
		goto error_exit_lrt;
	}

	// Set it to zero, so that we know if it ever got initialized
	memset(verbs_processes, 0, sizeof(ProcessInfo) * (_photon_nproc+_photon_forwarder));

	for(i = 0; i < _photon_nproc+_photon_forwarder; i++) {
		verbs_processes[i].curr_remote_buffer = verbs_remote_buffer_create();
		if(!verbs_processes[i].curr_remote_buffer) {
			log_err("Couldn't allocate process remote buffer information");
			goto error_exit_gp;
		}
	}

	dbg_info("verbs_init_common(): alloc'd process info");

	if( verbs_init_context(verbs_processes) ) {
		log_err("verbs_init_common(): Couldn't initialize verbs context\n");
		goto error_exit_crb;
	}

	// Everything is x2 cause we need a local and a remote copy of each ledger.
	// Remote Info (_ri_) ledger has an additional x2 cause we need "send-info" and "receive-info" ledgers.
	info_ledger_size = 2 * 2 * sizeof(verbs_ri_ledger_entry_t) * LEDGER_SIZE * (_photon_nproc+_photon_forwarder);
	FIN_ledger_size  = 2 * sizeof(verbs_rdma_FIN_ledger_entry_t) * LEDGER_SIZE * (_photon_nproc+_photon_forwarder);
	bufsize = info_ledger_size + FIN_ledger_size;
	buf = malloc(bufsize);
	if (!buf) {
		log_err("verbs_init_common(): Couldn't allocate ledgers");
		goto error_exit_verbs_cnct;
	}
	dbg_info("Bufsize: %d", bufsize);

	shared_storage = verbs_buffer_create(buf, bufsize);
	if (!shared_storage) {
		log_err("verbs_init_common(): Couldn't register shared storage");
		goto error_exit_buf;
	}

	if (verbs_buffer_register(shared_storage, phot_verbs_pd) != 0) {
		log_err("verbs_init_common(): couldn't register local buffer for the ledger entries");
		goto error_exit_ss;
	}

	if (verbs_setup_ri_ledgers(buf, LEDGER_SIZE) != 0) {
		log_err("verbs_init_common(); couldn't setup snd/rcv info ledgers");
		goto error_exit_listeners;
	}

	// skip 4 ledgers (rcv info local, rcv info remote, snd info local, snd info remote)
	offset = 4 * sizeof(verbs_ri_ledger_entry_t) * LEDGER_SIZE * (_photon_nproc+_photon_forwarder);
	if (verbs_setup_FIN_ledger(buf + offset, LEDGER_SIZE) != 0) {
		log_err("verbs_init_common(); couldn't setup send ledgers");
		goto error_exit_ri_ledger;
	}

#ifdef PHOTON_MULTITHREADED
	if (pthread_create(&ledger_watcher, NULL, verbs_req_watcher, NULL)) {
		log_err("verbs_init_common(): pthread_create() failed.\n");
		goto error_exit_ledger_watcher;
	}
#endif

	dbg_info("verbs_init_common(): ended successfully =============");

	return 0;

#ifdef PHOTON_MULTITHREADED
error_exit_ledger_watcher:
#endif
error_exit_ri_ledger:
error_exit_listeners:
error_exit_ss:
	verbs_buffer_free(shared_storage);
error_exit_buf:
	if (buf)
		free(buf);
error_exit_verbs_cnct:
error_exit_crb:
	for(i = 0; i < _photon_nproc+_photon_forwarder; i++) {
		if (verbs_processes[i].curr_remote_buffer != NULL) {
			verbs_remote_buffer_free(verbs_processes[i].curr_remote_buffer);
		}
	}
error_exit_gp:
	free(verbs_processes);
error_exit_lrt:
	htable_free(ledger_reqtable);
error_exit_rt:
	htable_free(reqtable);
error_exit_bt:
	buffertable_finalize();
error_exit_req:
	free(requests);
	DESTROY_COUNTER(curr_cookie);
	DESTROY_COUNTER(handshake_rdma_write);
error_exit:
	__initialized = 0;
	return -1;
}

int verbs_init(photonConfig cfg) {
	int i;

	if (verbs_init_common(cfg) != 0)
		goto error_exit;

	if( verbs_connect_peers(verbs_processes) ) {
		log_err("verbs_init(): Couldn't exchange peer information to create the connections\n");
		goto error_exit_verbs_cntx;
	}

	if (verbs_exchange_ri_ledgers() != 0) {
		log_err("verbs_init(); couldn't exchange rdma ledgers");
		goto error_exit_listeners;
	}

	if (verbs_exchange_FIN_ledger() != 0) {
		log_err("verbs_init(); couldn't exchange send ledgers");
		goto error_exit_FIN_ledger;
	}

	while( !SLIST_EMPTY(&pending_mem_register_list) ) {
		struct mem_register_req *mem_reg_req;
		dbg_info("verbs_init(): registering buffer in queue");
		mem_reg_req = SLIST_FIRST(&pending_mem_register_list);
		SLIST_REMOVE_HEAD(&pending_mem_register_list, list);
		// FIXME: What if this fails?
		verbs_register_buffer(mem_reg_req->buffer, mem_reg_req->buffer_size);
	}

#ifdef WITH_XSP
	if (forwarder) {

		_photon_fp = nproc;
		sess_count = 1;
		pthread_mutex_init(&sess_mtx, NULL);

		if (verbs_xsp_init() != 0) {
			log_err("verbs_init(); couldn't initialize phorwarder connection");
			goto error_exit_FIN_ledger;
		}
	}
#endif

	__initialized = 1;
	dbg_info("verbs_init(): ended successfully =============");

	return 0;

error_exit_FIN_ledger:
error_exit_listeners:
error_exit_verbs_cntx:
	if (shared_storage->buffer)
		free(shared_storage->buffer);
	verbs_buffer_free(shared_storage);
	for(i = 0; i < _photon_nproc+_photon_forwarder; i++) {
		if (verbs_processes[i].curr_remote_buffer != NULL) {
			verbs_remote_buffer_free(verbs_processes[i].curr_remote_buffer);
		}
	}
	free(verbs_processes);
	htable_free(ledger_reqtable);
	htable_free(reqtable);
	buffertable_finalize();
	free(requests);
	DESTROY_COUNTER(curr_cookie);
	DESTROY_COUNTER(handshake_rdma_write);
	__initialized = 0;
error_exit:
	return -1;
}

///////////////////////////////////////////////////////////////////////////////
int verbs_finalize() {
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
static int verbs_exchange_ri_ledgers() {
	int i;
	MPI_Request *req;
	uintptr_t *va;

	ctr_info(" > verbs_exchange_ri_ledgers()");

	if( __initialized != -1 ) {
		log_err("verbs_exchange_ri_ledgers(): Library not initialized.  Call photon_init() first");
		return -1;
	}

	va = (uintptr_t *)malloc( _photon_nproc*sizeof(uintptr_t) );
	req = (MPI_Request *)malloc( 2*_photon_nproc*sizeof(MPI_Request) );
	if( !va || !req ) {
		log_err("verbs_exchange_ri_ledgers(): Cannot malloc temporary message buffers\n");
		return -1;
	}
	memset(va, 0, _photon_nproc*sizeof(uintptr_t));
	memset(req, 0, 2*_photon_nproc*sizeof(MPI_Request));

///////////////////////////////////////////////////////////////////////////////
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


///////////////////////////////////////////////////////////////////////////////
static int verbs_setup_ri_ledgers(char *buf, int num_entries) {
	int i;
	int ledger_size, offset;

	ctr_info(" > verbs_setup_ri_ledgers()");

	if( __initialized != -1 ) {
		log_err("verbs_setup_ri_ledgers(): Library not initialized.  Call photon_init() first");
		return -1;
	}

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


///////////////////////////////////////////////////////////////////////////////
static int verbs_exchange_FIN_ledger() {
	int i;
	uintptr_t   *va;
	MPI_Request *req;

	ctr_info(" > verbs_exchange_FIN_ledger()");

	if( __initialized != -1 ) {
		log_err("verbs_exchange_FIN_ledger(): Library not initialized.  Call photon_init() first");
		return -1;
	}

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


///////////////////////////////////////////////////////////////////////////////
static int verbs_setup_FIN_ledger(char *buf, int num_entries) {
	int i;
	int ledger_size;

	ctr_info(" > verbs_setup_FIN_ledger()");

	if( __initialized != -1 ) {
		log_err("verbs_setup_FIN_ledger(): Library not initialized.	 Call photon_init() first");
		return -1;
	}

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


///////////////////////////////////////////////////////////////////////////////
int verbs_register_buffer(char *buffer, int buffer_size) {
	static int first_time = 1;
	verbs_buffer_t *db;

	ctr_info(" > verbs_register_buffer(%p, %d)",buffer, buffer_size);

	if( __initialized == 0 ) {
		struct mem_register_req *mem_reg_req;
		if( first_time ) {
			SLIST_INIT(&pending_mem_register_list);
			first_time = 0;
		}
		mem_reg_req = malloc( sizeof(struct mem_register_req) );
		mem_reg_req->buffer = buffer;
		mem_reg_req->buffer_size = buffer_size;

		SLIST_INSERT_HEAD(&pending_mem_register_list, mem_reg_req, list);
		dbg_info("verbs_register_buffer(): called before init, queueing buffer info");
		goto normal_exit;
	}

	if (buffertable_find_exact((void *)buffer, buffer_size, &db) == 0) {
		dbg_info("verbs_register_buffer(): we had an existing buffer, reusing it");
		db->ref_count++;
		goto normal_exit;
	}

	db = verbs_buffer_create(buffer, buffer_size);
	if (!db) {
		log_err("Couldn't register shared storage");
		goto error_exit;
	}

	dbg_info("verbs_register_buffer(): created buffer: %p", db);

	if (verbs_buffer_register(db, phot_verbs_pd) != 0) {
		log_err("Couldn't register buffer");
		goto error_exit_db;
	}

	dbg_info("verbs_register_buffer(): registered buffer");

	if (buffertable_insert(db) != 0) {
		goto error_exit_db;
	}

	dbg_info("verbs_register_buffer(): added buffer to hash table");

normal_exit:
	return 0;
error_exit_db:
	verbs_buffer_free(db);
error_exit:
	return -1;
}


///////////////////////////////////////////////////////////////////////////////
int verbs_unregister_buffer(char *buffer, int size) {
	verbs_buffer_t *db;

	ctr_info(" > verbs_unregister_buffer()");

	if( __initialized == 0 ) {
		log_err("verbs_unregister_buffer(): Library not initialized.	Call photon_init() first");
		goto error_exit;
	}

	if (buffertable_find_exact((void *)buffer, size, &db) != 0) {
		dbg_info("verbs_unregister_buffer(): no such buffer is registered");
		return 0;
	}

	if (--(db->ref_count) == 0) {
		if (verbs_buffer_unregister(db) != 0) {
			goto error_exit;
		}
		buffertable_remove( db );
		verbs_buffer_free(db);
	}

	return 0;

error_exit:
	return -1;
}



///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//					 Some Utility Functions to wait for specific events							 //
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// verbs_test() is a nonblocking operation that checks the event queue to see if
// the event associated with the "request" parameter has completed.	 It returns:
//	0 if the event associated with "request" was in the queue and was successfully poped.
//	1 if "request" was not in the request tables.	 This is not an error if verbs_test()
//		is called in a loop and is called multiple times for each request.
// -1 if an error occured.
//
// When verbs_test() returns zero (success) the "flag" parameter has the value:
//	0 if the event that was poped does not correspond to "request", or if none of the operations completed.
//	1 if the event that was poped does correspond to "request".
//
//	When verbs_test() returns 0 and flag==0 the "status" structure is also filled
//	unless the constant "MPI_STATUS_IGNORE" was passed as the "status" argument.
//
// Regardless of the return value and the value of "flag", the parameter "type"
// will be set to 0 (zero) when the request is of type event and 1 (one) when the
// request is of type ledger.
int verbs_test(uint32_t request, int *flag, int *type, MPI_Status *status) {
	verbs_req_t *req;
	void *test;
	int ret_val;

	ctr_info(" > verbs_test(%d)",request);

	if( __initialized <= 0 ) {
		log_err("verbs_test(): Library not initialized.	 Call photon_init() first");
		dbg_info("verbs_test(): returning -1");
		return -1;
	}

	if (htable_lookup(reqtable, (uint64_t)request, &test) != 0) {
		if (htable_lookup(ledger_reqtable, (uint64_t)request, &test) != 0) {
			dbg_info("Request is not in either request-table");
			// Unlike verbs_wait(), we might call verbs_test() multiple times on a request,
			// e.g., in an unguarded loop.	flag==-1 will signify that the operation is
			// not pending.	 This means, it might be completed, it might have never been
			// issued.	It's up to the application to guarantee correctness, by keeping
			// track, of	what's going on.	Unless you know what you are doing, consider
			// (flag==-1 && return_value==1) to be an error case.
			dbg_info("verbs_test(): returning 1, flag:-1");
			*flag = -1;
			return 1;
		}
	}

	req = test;

	*flag = 0;

#ifdef PHOTON_MULTITHREADED
	pthread_mutex_lock(&req->mtx);
	{
		ret_val = (req->state == REQUEST_COMPLETED)?0:1;
	}
	pthread_mutex_unlock(&req->mtx);
#else
	if (req->type == LEDGER) {
		if( type != NULL ) *type = 1;
		ret_val = __verbs_nbpop_ledger(req);
	}
	else {
		if( type != NULL ) *type = 0;
		ret_val = __verbs_nbpop_event(req);
	}
#endif

	if( !ret_val ) {
		*flag = 1;
		if( status != MPI_STATUS_IGNORE ) {
			status->MPI_SOURCE = req->proc;
			status->MPI_TAG = req->tag;
			status->MPI_ERROR = 0; // FIXME: Make sure that "0" means success in MPI?
		}
		dbg_info("verbs_test(): returning 0, flag:1");
		return 0;
	}
	else if( ret_val > 0 ) {
		dbg_info("verbs_test(): returning 0, flag:0");
		*flag = 0;
		return 0;
	}
	else {
		dbg_info("verbs_test(): returning -1, flag:0");
		*flag = 0;
		return -1;
	}
}

///////////////////////////////////////////////////////////////////////////////
int verbs_wait(uint32_t request) {
	verbs_req_t *req;

	ctr_info(" > verbs_wait(%d)",request);

	if( __initialized <= 0 ) {
		log_err("verbs_wait(): Library not initialized.	 Call photon_init() first");
		return -1;
	}

	if (htable_lookup(reqtable, (uint64_t)request, (void**)&req) != 0) {
		if (htable_lookup(ledger_reqtable, (uint64_t)request, (void**)&req) != 0) {
			log_err("verbs_wait(): Wrong request value, operation not in table");
			return -1;
		}
	}

#ifdef PHOTON_MULTITHREADED

	pthread_mutex_lock(&req->mtx);
	{
		while(req->state == REQUEST_PENDING)
			pthread_cond_wait(&req->completed, &req->mtx);

		if (req->type == LEDGER) {
			if (htable_lookup(ledger_reqtable, (uint64_t)req->id, NULL) != -1) {
				dbg_info("verbs_wait(): removing ledger RDMA: %u", req->id);
				htable_remove(ledger_reqtable, (uint64_t)req->id, NULL);
				SAFE_LIST_REMOVE(req, list);
				SAFE_LIST_INSERT_HEAD(&free_reqs_list, req, list);
				dbg_info("verbs_wait(): %d requests left in ledgertable", htable_count(ledger_reqtable));
			}
		}
		else {
			if (htable_lookup(reqtable, (uint64_t)req->id, NULL) != -1) {
				dbg_info("verbs_wait(): removing event with cookie:%u", req->id);
				htable_remove(reqtable, (uint64_t)req->id, NULL);
				SAFE_LIST_REMOVE(req, list);
				SAFE_LIST_INSERT_HEAD(&free_reqs_list, req, list);
				dbg_info("verbs_wait(): %d requests left in reqtable", htable_count(reqtable));
			}
		}
	}
	pthread_mutex_unlock(&req->mtx);

	return (req->state == REQUEST_COMPLETED)?0:-1;

#else
	if (req->type == LEDGER)
		return __verbs_wait_ledger(req);
	else
		return __verbs_wait_event(req);
#endif
}

#ifndef PHOTON_MULTITHREADED
///////////////////////////////////////////////////////////////////////////////
// this function can be used to wait for an event to occur
static int __verbs_wait_one() {
	uint32_t cookie;
	verbs_req_t *tmp_req;
	void *test;
	int ne;
	struct ibv_wc wc;

	dbg_info("__verbs_wait_one(): remaining: %d+%d", htable_count(reqtable), GET_COUNTER(handshake_rdma_write));

	if( __initialized <= 0 ) {
		log_err("__verbs_wait_one(): Library not initialized.	 Call photon_init() first");
		goto error_exit;
	}

	if ( (htable_count(reqtable) == 0) && (GET_COUNTER(handshake_rdma_write) == 0) ) {
		dbg_info("No events on queue, or handshake writes pending to wait on");
		goto error_exit;
	}

	do {
		ne = ibv_poll_cq(phot_verbs_cq, 1, &wc);
		if (ne < 0) {
			log_err("ibv_poll_cq() failed");
			goto error_exit;
		}
	}
	while (ne < 1);

	if (wc.status != IBV_WC_SUCCESS) {
		log_err("__verbs_wait_event(): (status==%d) != IBV_WC_SUCCESS\n",wc.status);
		goto error_exit;
	}

	cookie = (uint32_t)( (wc.wr_id<<32)>>32);
	dbg_info("__verbs_wait_one(): received event with cookie:%u", cookie);

	if (htable_lookup(reqtable, (uint64_t)cookie, &test) == 0) {
		tmp_req = test;

		tmp_req->state = REQUEST_COMPLETED;
		SAFE_LIST_REMOVE(tmp_req, list);
		SAFE_LIST_INSERT_HEAD(&unreaped_evd_reqs_list, tmp_req, list);
	}
	else if (htable_lookup(ledger_reqtable, (uint64_t)cookie, &test) == 0) {
		if( DEC_COUNTER(handshake_rdma_write) <= 0 ) {
			log_err("__verbs_wait_one(): handshake_rdma_write_count is negative");
		}
	}

	return 0;
error_exit:
	return -1;
}


///////////////////////////////////////////////////////////////////////////////
static int __verbs_wait_event(verbs_req_t *req) {
	int ne;
	struct ibv_wc wc;

	if( __initialized <= 0 ) {
		log_err("__verbs_wait_event(): Library not initialized.	 Call photon_init() first");
		return -1;
	}


//fprintf(stderr,"[%d/%d] __verbs_wait_event(): req->state:%d, req->id:%d\n", _photon_myrank, _photon_nproc, req->state, req->id);

// I think here we should check if the request is in the unreaped_evd_reqs_list
// (i.e. already completed) and if so, move it to the free_reqs_list:
//		if(req->state == REQUEST_COMPLETED){
//				LIST_REMOVE(req, list);
//				LIST_INSERT_HEAD(&free_reqs_list, req, list);
//		}

	while(req->state == REQUEST_PENDING) {
		uint32_t cookie;
//				int i=0,j=0;

		do {
			ne = ibv_poll_cq(phot_verbs_cq, 1, &wc);
			if (ne < 0) {
				fprintf(stderr, "poll CQ failed %d\n", ne);
				return 1;
			}
		}
		while (ne < 1);

		if (wc.status != IBV_WC_SUCCESS) {
			log_err("__verbs_wait_event(): (status==%d) != IBV_WC_SUCCESS\n",wc.status);
			goto error_exit;
		}

		cookie = (uint32_t)( (wc.wr_id<<32)>>32);
		//fprintf(stderr,"[%d/%d] __verbs_wait_event(): Event occured with cookie:%x\n", _photon_myrank, _photon_nproc, cookie);
		if (cookie == req->id) {
			req->state = REQUEST_COMPLETED;

			dbg_info("verbs_wait_event(): removing event with cookie:%u", cookie);
			htable_remove(reqtable, (uint64_t)req->id, NULL);
			SAFE_LIST_REMOVE(req, list);
			SAFE_LIST_INSERT_HEAD(&free_reqs_list, req, list);
			dbg_info("verbs_wait_evd(): %d requests left in reqtable", htable_count(reqtable));
		}
		else if (cookie != NULL_COOKIE) {
			void *test;

			if (htable_lookup(reqtable, (uint64_t)cookie, &test) == 0) {
				verbs_req_t *tmp_req = test;

				tmp_req->state = REQUEST_COMPLETED;
				SAFE_LIST_REMOVE(tmp_req, list);
				SAFE_LIST_INSERT_HEAD(&unreaped_evd_reqs_list, tmp_req, list);
			}
			else if (htable_lookup(ledger_reqtable, (uint64_t)cookie, &test) == 0) {
				if( DEC_COUNTER(handshake_rdma_write) <= 0 ) {
					log_err("__verbs_wait_event(): handshake_rdma_write_count is negative");
				}
			}
		}
	}

	return (req->state == REQUEST_COMPLETED)?0:-1;

error_exit:
	return -1;
}


///////////////////////////////////////////////////////////////////////////////
// __verbs_nbpop_event() is non blocking and returns:
// -1 if an error occured.
//	0 if the request (req) specified in the argument has completed.
//	1 if either no event was in the queue, or there was an event but not for the specified request (req).
static int __verbs_nbpop_event(verbs_req_t *req) {
	int ne;
	struct ibv_wc wc;

	ctr_info(" > __verbs_nbpop_event(%d)",req->id);

	if( __initialized <= 0 ) {
		log_err("__verbs_nbpop_event(): Library not initialized.	Call photon_init() first");
		return -1;
	}

	if(req->state == REQUEST_PENDING) {
		uint32_t cookie;

		ne = ibv_poll_cq(phot_verbs_cq, 1, &wc);
		if (ne < 0) {
			log_err("ibv_poll_cq() failed");
			goto error_exit;
		}
		if (!ne) {
			return 1;
		}

		if (wc.status != IBV_WC_SUCCESS) {
			log_err("__verbs_wait_event(): (status==%d) != IBV_WC_SUCCESS\n",wc.status);
			goto error_exit;
		}

		cookie = (uint32_t)( (wc.wr_id<<32)>>32);
//fprintf(stderr,"__gen_nbpop_event() poped an events with cookie:%x\n",cookie);
		if (cookie == req->id) {
			req->state = REQUEST_COMPLETED;

			dbg_info("verbs_wait_event(): removing event with cookie:%u", cookie);
			htable_remove(reqtable, (uint64_t)req->id, NULL);
			SAFE_LIST_REMOVE(req, list);
			SAFE_LIST_INSERT_HEAD(&free_reqs_list, req, list);
			dbg_info("verbs_wait_evd(): %d requests left in reqtable", htable_count(reqtable));
		}
		else if (cookie != NULL_COOKIE) {
			void *test;

			if (htable_lookup(reqtable, (uint64_t)cookie, &test) == 0) {
				verbs_req_t *tmp_req = test;

				tmp_req->state = REQUEST_COMPLETED;
				SAFE_LIST_REMOVE(tmp_req, list);
				SAFE_LIST_INSERT_HEAD(&unreaped_evd_reqs_list, tmp_req, list);
			}
			else if (htable_lookup(ledger_reqtable, (uint64_t)cookie, &test) == 0) {
				if( DEC_COUNTER(handshake_rdma_write) <= 0 ) {
					log_err("__verbs_wait_event(): handshake_rdma_write_count is negative");
				}
			}
		}
	}

	dbg_info("__verbs_nbpop_event(): returning %d", (req->state == REQUEST_COMPLETED)?0:1 );
	return (req->state == REQUEST_COMPLETED)?0:1;

error_exit:
	return -1;
}

///////////////////////////////////////////////////////////////////////////////
static int __verbs_wait_ledger(verbs_req_t *req) {
	void *test;
	int curr, num_entries, i=-1;

	ctr_info(" > __verbs_wait_ledger(%d)",req->id);

	if( __initialized <= 0 ) {
		log_err("__verbs_wait_ledger(): Library not initialized.	Call photon_init() first");
		return -1;
	}

#ifdef DEBUG
	for(i = 0; i < _photon_nproc; i++) {
		verbs_rdma_FIN_ledger_entry_t *curr_entry;
		curr = verbs_processes[i].local_FIN_ledger->curr;
		curr_entry = &(verbs_processes[i].local_FIN_ledger->entries[curr]);
		dbg_info("__verbs_wait_ledger() curr_entry(proc==%d)=%p",i,curr_entry);
	}
#endif
	while(req->state == REQUEST_PENDING) {

		// Check if an entry of the FIN LEDGER was written with "id" equal to to "req"
		for(i = 0; i < _photon_nproc; i++) {
			verbs_rdma_FIN_ledger_entry_t *curr_entry;
			curr = verbs_processes[i].local_FIN_ledger->curr;
			curr_entry = &(verbs_processes[i].local_FIN_ledger->entries[curr]);
			if (curr_entry->header != (uint8_t) 0 && curr_entry->footer != (uint8_t) 0) {
				dbg_info("__verbs_wait_ledger() Found: %d/%u/%u", curr, curr_entry->request, req->id);
				curr_entry->header = 0;
				curr_entry->footer = 0;

				if (curr_entry->request == req->id) {
					req->state = REQUEST_COMPLETED;
				}
				else {
					verbs_req_t *tmp_req;

					if (htable_lookup(ledger_reqtable, (uint64_t)curr_entry->request, &test) == 0) {
						tmp_req = test;

						tmp_req->state = REQUEST_COMPLETED;
						SAFE_LIST_REMOVE(tmp_req, list);
						SAFE_LIST_INSERT_HEAD(&unreaped_ledger_reqs_list, tmp_req, list);
					}
				}

				num_entries = verbs_processes[i].local_FIN_ledger->num_entries;
				curr = verbs_processes[i].local_FIN_ledger->curr;
				curr = (curr + 1) % num_entries;
				verbs_processes[i].local_FIN_ledger->curr = curr;
			}
		}
	}
	dbg_info("verbs_wait_ledger(): removing RDMA: %u/%u", i, req->id);
	htable_remove(ledger_reqtable, (uint64_t)req->id, NULL);
	SAFE_LIST_REMOVE(req, list);
	SAFE_LIST_INSERT_HEAD(&free_reqs_list, req, list);
	dbg_info("verbs_wait_ledger(): %d requests left in reqtable", htable_count(ledger_reqtable));

	return (req->state == REQUEST_COMPLETED)?0:-1;
}

///////////////////////////////////////////////////////////////////////////////
// returns
// -1 if an error occured.
//	0 if the FIN associated with "req" was found and poped, or
//		the "req" is not pending.	 This is not an error, if a previous
//		call to __verbs_nbpop_ledger() poped the FIN that corresponds to "req".
//	1 if the request is pending and the FIN has not arrived yet
static int __verbs_nbpop_ledger(verbs_req_t *req) {
	void *test;
	int curr, i=-1;

	ctr_info(" > __verbs_nbpop_ledger(%d)",req->id);

	if( __initialized <= 0 ) {
		log_err("__verbs_nbpop_ledger(): Library not initialized.	 Call photon_init() first");
		return -1;
	}

//#ifdef DEBUG
//		for(i = 0; i < _photon_nproc; i++) {
//				verbs_rdma_FIN_ledger_entry_t *curr_entry;
//				curr = verbs_processes[i].local_FIN_ledger->curr;
//				curr_entry = &(verbs_processes[i].local_FIN_ledger->entries[curr]);
//				dbg_info("__verbs_nbpop_ledger() curr_entry(proc==%d)=%p",i,curr_entry);
//		}
//#endif

	if(req->state == REQUEST_PENDING) {

		// Check if an entry of the FIN LEDGER was written with "id" equal to to "req"
		for(i = 0; i < _photon_nproc; i++) {
			verbs_rdma_FIN_ledger_entry_t *curr_entry;
			curr = verbs_processes[i].local_FIN_ledger->curr;
			curr_entry = &(verbs_processes[i].local_FIN_ledger->entries[curr]);
			if (curr_entry->header != (uint8_t) 0 && curr_entry->footer != (uint8_t) 0) {
				dbg_info("__verbs_nbpop_ledger(): Found curr:%d req:%u while looking for req:%u", curr, curr_entry->request, req->id);
				curr_entry->header = 0;
				curr_entry->footer = 0;

				if (curr_entry->request == req->id) {
					req->state = REQUEST_COMPLETED;
					dbg_info("__verbs_nbpop_ledger(): removing RDMA i:%u req:%u", i, req->id);
					htable_remove(ledger_reqtable, (uint64_t)req->id, NULL);
					LIST_REMOVE(req, list);
					LIST_INSERT_HEAD(&free_reqs_list, req, list);
					int num = verbs_processes[i].local_FIN_ledger->num_entries;
					int new_curr = (verbs_processes[i].local_FIN_ledger->curr + 1) % num;
					verbs_processes[i].local_FIN_ledger->curr = new_curr;
					dbg_info("__verbs_nbpop_ledger(): returning 0");
					return 0;
				}
				else {
					verbs_req_t *tmp_req;

					if (htable_lookup(ledger_reqtable, (uint64_t)curr_entry->request, &test) == 0) {
						tmp_req = test;

						tmp_req->state = REQUEST_COMPLETED;
						LIST_REMOVE(tmp_req, list);
						LIST_INSERT_HEAD(&unreaped_ledger_reqs_list, tmp_req, list);
					}
				}

				int num = verbs_processes[i].local_FIN_ledger->num_entries;
				int new_curr = (verbs_processes[i].local_FIN_ledger->curr + 1) % num;
				verbs_processes[i].local_FIN_ledger->curr = new_curr;
			}
		}
	}
	else {
		dbg_info("__verbs_nbpop_ledger(): req->state != PENDING, returning 0");
		return 0;
	}

	dbg_info("__verbs_nbpop_ledger(): at end, returning %d",(req->state == REQUEST_COMPLETED)?0:1);
	return (req->state == REQUEST_COMPLETED)?0:1;
}

///////////////////////////////////////////////////////////////////////////////
int verbs_wait_any(int *ret_proc, uint32_t *ret_req) {
	struct ibv_wc wc;

	dbg_info("verbs_wait_any(): remaining: %d", htable_count(reqtable));

	if( __initialized <= 0 ) {
		log_err("verbs_wait_any(): Library not initialized.	 Call photon_init() first");
		goto error_exit;
	}

	if (ret_req == NULL) {
		goto error_exit;
	}

	if (htable_count(reqtable) == 0) {
		log_err("verbs_wait_any(): No events on queue to wait on");
		goto error_exit;
	}

	while(1) {
		uint32_t cookie;
		int existed, ne;

		ne = ibv_poll_cq(phot_verbs_cq, 1, &wc);
		if (ne < 0) {
			log_err("ibv_poll_cq() failed");
			goto error_exit;
		}
		if (!ne) {
			goto error_exit;
		}

		if (wc.status != IBV_WC_SUCCESS) {
			log_err("__verbs_wait_event(): (status==%d) != IBV_WC_SUCCESS\n",wc.status);
			goto error_exit;
		}

		cookie = (uint32_t)( (wc.wr_id<<32)>>32);
//fprintf(stderr,"gen_wait_any() poped an events with cookie:%x\n",cookie);
		if (cookie != NULL_COOKIE) {
			verbs_req_t *req;
			void *test;

			dbg_info("verbs_wait_any(): removing event with cookie:%u", cookie);
			existed = htable_remove(ledger_reqtable, (uint64_t)cookie, &test);
			req = test;
			SAFE_LIST_REMOVE(req, list);
			SAFE_LIST_INSERT_HEAD(&free_reqs_list, req, list);
		}
		else {
			existed = -1;
		}

		if (existed == -1) {
			continue;
		}
		else {
			*ret_req = cookie;
			*ret_proc = (uint32_t)(wc.wr_id>>32);
			return 0;
		}
	}

	return 0;
error_exit:
	return -1;
}


///////////////////////////////////////////////////////////////////////////////
int verbs_wait_any_ledger(int *ret_proc, uint32_t *ret_req) {
	static int i = -1; // this is static so we don't starve events in later processes
	int curr, num_entries;

	dbg_info("verbs_wait_any(): remaining: %d", htable_count(reqtable));

	if( __initialized <= 0 ) {
		log_err("verbs_wait_any_ledger(): Library not initialized.	Call photon_init() first");
		return -1;
	}

	if (ret_req == NULL || ret_proc == NULL) {
		return -1;
	}

	if (htable_count(ledger_reqtable) == 0) {
		log_err("verbs_wait_any_ledger(): No events on queue to wait_one()");
		return -1;
	}

	while(1) {
		verbs_rdma_FIN_ledger_entry_t *curr_entry;
		int exists;

		i=(i+1)%_photon_nproc;

		// check if an event occurred on the RDMA end of things
		curr = verbs_processes[i].local_FIN_ledger->curr;
		curr_entry = &(verbs_processes[i].local_FIN_ledger->entries[curr]);

		if (curr_entry->header != (uint8_t) 0 && curr_entry->footer != (uint8_t) 0) {
			void *test;
			dbg_info("Wait All In: %d/%u", verbs_processes[i].local_FIN_ledger->curr, curr_entry->request);
			curr_entry->header = 0;
			curr_entry->footer = 0;

			exists = htable_remove(ledger_reqtable, (uint64_t)curr_entry->request, &test);
			if (exists != -1) {
				verbs_req_t *req;
				req = test;
				*ret_req = curr_entry->request;
				*ret_proc = i;
				SAFE_LIST_REMOVE(req, list);
				SAFE_LIST_INSERT_HEAD(&free_reqs_list, req, list);
				break;
			}

			num_entries = verbs_processes[i].local_FIN_ledger->num_entries;
			curr = verbs_processes[i].local_FIN_ledger->curr;
			curr = (curr + 1) % num_entries;
			verbs_processes[i].local_FIN_ledger->curr = curr;
			dbg_info("Wait All Out: %d", curr);
		}
	}

	return 0;
}

#else /* PHOTON_MULTITHREADED */
///////////////////////////////////////////////////////////////////////////////
static inline int __verbs_complete_ledger_req(uint32_t cookie) {
	verbs_req_t *tmp_req;

	if (htable_lookup(ledger_reqtable, (uint64_t)cookie, (void**)&tmp_req) != 0)
		return -1;

	dbg_info("__verbs_complete_ledger_req(): completing ledger req %"PRIo32, cookie);
	pthread_mutex_lock(&tmp_req->mtx);
	{
		tmp_req->state = REQUEST_COMPLETED;
		SAFE_LIST_REMOVE(tmp_req, list);
		SAFE_LIST_INSERT_HEAD(&unreaped_ledger_reqs_list, tmp_req, list);
		pthread_cond_broadcast(&tmp_req->completed);
	}
	pthread_mutex_unlock(&tmp_req->mtx);

	return 0;
}

static inline int __verbs_complete_evd_req(uint32_t cookie) {
	verbs_req_t *tmp_req;

	if (htable_lookup(reqtable, (uint64_t)cookie, (void**)&tmp_req) != 0)
		return -1;

	dbg_info("__verbs_complete_ledger_req(): completing event req %"PRIo32, cookie);
	pthread_mutex_lock(&tmp_req->mtx);
	{
		tmp_req->state = REQUEST_COMPLETED;
		SAFE_LIST_REMOVE(tmp_req, list);
		SAFE_LIST_INSERT_HEAD(&unreaped_evd_reqs_list, tmp_req, list);
		pthread_cond_broadcast(&tmp_req->completed);
	}
	pthread_mutex_unlock(&tmp_req->mtx);

	return 0;
}

static void *verbs_req_watcher(void *arg) {
	int i;
	int ne;
	int curr;
	uint32_t cookie;
	struct ibv_wc wc[32];

	ctr_info(" > reqs watcher started.");

	if( __initialized == 0 ) {
		log_err("verbs_req_watcher(): Library not initialized.	Call photon_init() first");
		pthread_exit((void*)-1);
	}

	// FIXME: Should run only when there's something to watch for (ledger/event).
	while(1) {
		// First we poll for CQEs and clear reqs waiting on them.
		// We don't want to spend too much time on this before moving to ledgers.
		ne = ibv_poll_cq(phot_verbs_cq, 32, wc);
		if (ne < 0) {
			log_err("verbs_req_watcher(): poll CQ failed %d, EXITING WATCHER\n", ne);
			pthread_exit((void*)-1);
		}

		for (i = 0; i < ne; i++) {
			if (wc[i].status != IBV_WC_SUCCESS) {
				// TODO: is there anything we can/must do with the error?
				//	 I think the wr_id is valid, so we should probably notify the request.
				log_err("verbs_req_watcher(): (status==%d) != IBV_WC_SUCCESS\n",wc[i].status);
				continue;
			}

			cookie = (uint32_t)( (wc[i].wr_id<<32)>>32);

			if (cookie == NULL_COOKIE)
				continue;

			if (!__verbs_complete_ledger_req(cookie))
				continue;

			if (!__verbs_complete_evd_req(cookie))
				continue;

			// TODO: Is this the only other possibility?
			if( DEC_COUNTER(handshake_rdma_write) <= 0 )
				log_err("verbs_req_watcher(): handshake_rdma_write_count is negative");
		}

		for(i = 0; i < _photon_nproc; i++) {
			verbs_rdma_FIN_ledger_entry_t *curr_entry;
			curr = verbs_processes[i].local_FIN_ledger->curr;
			curr_entry = &(verbs_processes[i].local_FIN_ledger->entries[curr]);
			if (curr_entry->header != (uint8_t) 0 && curr_entry->footer != (uint8_t) 0) {
				dbg_info("verbs_req_watcher() found: %d/%u", curr, curr_entry->request);
				curr_entry->header = 0;
				curr_entry->footer = 0;

				if (__verbs_complete_ledger_req(curr_entry->request))
					log_err("verbs_req_watcher(): couldn't find req for FIN ledger: %u", curr_entry->request);

				verbs_processes[i].local_FIN_ledger->curr = (verbs_processes[i].local_FIN_ledger->curr + 1) % verbs_processes[i].local_FIN_ledger->num_entries;
				dbg_info("verbs_req_watcher(): %d requests left in reqtable", htable_count(ledger_reqtable));
			}
		}
	}

	pthread_exit(NULL);
}

#endif

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
//								DAPL One-Sided send()s/recv()s and handshake functions								//
//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
int verbs_wait_recv_buffer_rdma(int proc, int tag) {
	verbs_remote_buffer_t *curr_remote_buffer;
	verbs_ri_ledger_entry_t *curr_entry, *entry_iterator, tmp_entry;
	int count;
#ifdef DEBUG
	time_t stime;
#endif
	int curr, num_entries, still_searching;

	ctr_info(" > verbs_wait_recv_buffer_rdma(%d, %d)", proc, tag);

	if( __initialized <= 0 ) {
		log_err("verbs_wait_recv_buffer_rdma(): Library not initialized.	Call photon_init() first");
		return -1;
	}

	// If we've received a Rendezvous-Start from processor "proc" that is still pending
	curr_remote_buffer = verbs_processes[proc].curr_remote_buffer;
	if ( curr_remote_buffer->request != NULL_COOKIE ) {
		// If it is for the same tag, return without looking
		if ( curr_remote_buffer->tag == tag ) {
			goto normal_exit;
		}
		else {   // Otherwise it's an error.	We should never process a Rendezvous-Start before
			// fully serving the previous ones.
			goto error_exit;
		}
	}

	dbg_info("verbs_wait_recv_buffer_rdma(): Spinning on info ledger looking for receive request");
	dbg_info("verbs_wait_recv_buffer_rdma(): curr == %d", verbs_processes[proc].local_rcv_info_ledger->curr);


	curr = verbs_processes[proc].local_rcv_info_ledger->curr;
	curr_entry = &(verbs_processes[proc].local_rcv_info_ledger->entries[curr]);

	dbg_info("verbs_wait_recv_buffer_rdma(): looking in position %d/%p", verbs_processes[proc].local_rcv_info_ledger->curr, curr_entry);

#ifdef DEBUG
	stime = time(NULL);
#endif
	count = 1;
	still_searching = 1;
	entry_iterator = curr_entry;
	do {
		while(entry_iterator->header == 0 || entry_iterator->footer == 0) {
#ifdef DEBUG
			stime = _tictoc(stime, proc);
#else
			;
#endif
		}
		if( (tag < 0) || (entry_iterator->tag == tag ) ) {
			still_searching = 0;
		}
		else {
			curr = verbs_processes[proc].local_rcv_info_ledger->curr;
			num_entries = verbs_processes[proc].local_rcv_info_ledger->num_entries;
			curr = (curr + count++) % num_entries;
			entry_iterator = &(verbs_processes[proc].local_rcv_info_ledger->entries[curr]);
		}
	}
	while(still_searching);

	// If it wasn't the first pending receive request, swap the one we will serve ( entry_iterator) with
	// the first pending (curr_entry) in the info ledger, so that we can increment the current pointer
	// (verbs_processes[proc].local_rcv_info_ledger->curr) and skip the request we will serve without losing any
	// pending requests.
	if( entry_iterator != curr_entry ) {
		tmp_entry = *entry_iterator;
		*entry_iterator = *curr_entry;
		*curr_entry = tmp_entry;
	}

	verbs_processes[proc].curr_remote_buffer->request = curr_entry->request;
	verbs_processes[proc].curr_remote_buffer->rkey = curr_entry->rkey;
	verbs_processes[proc].curr_remote_buffer->addr = curr_entry->addr;
	verbs_processes[proc].curr_remote_buffer->size = curr_entry->size;
	verbs_processes[proc].curr_remote_buffer->tag	 = curr_entry->tag;

	dbg_info("verbs_wait_recv_buffer_rdma(): Request: %u", curr_entry->request);
	dbg_info("verbs_wait_recv_buffer_rdma(): rkey: %u", curr_entry->rkey);
	dbg_info("verbs_wait_recv_buffer_rdma(): Addr: %p", (void *)curr_entry->addr);
	dbg_info("verbs_wait_recv_buffer_rdma(): Size: %u", curr_entry->size);
	dbg_info("verbs_wait_recv_buffer_rdma(): Tag: %d",	curr_entry->tag);

	curr_entry->header = 0;
	curr_entry->footer = 0;

	num_entries = verbs_processes[proc].local_rcv_info_ledger->num_entries;
	curr = verbs_processes[proc].local_rcv_info_ledger->curr;
	curr = (curr + 1) % num_entries;
	verbs_processes[proc].local_rcv_info_ledger->curr = curr;

	dbg_info("verbs_wait_recv_buffer_rdma(): new curr == %d", verbs_processes[proc].local_rcv_info_ledger->curr);

normal_exit:
	return 0;
error_exit:
	return 1;

}



// verbs_wait_send_buffer_rdma() should never be called between a verbs_wait_recv_buffer_rdma()
// and the corresponding verbs_post_os_put(), or between an other verbs_wait_send_buffer_rdma()
// and the corresponding verbs_post_os_get() for the same proc.
// In other words if verbs_processes[proc].curr_remote_buffer is full, verbs_wait_send_buffer_rdma()
// should not be called.
int verbs_wait_send_buffer_rdma(int proc, int tag) {
	verbs_remote_buffer_t *curr_remote_buffer;
	verbs_ri_ledger_entry_t *curr_entry, *entry_iterator, tmp_entry;
	int count;
#ifdef DEBUG
	time_t stime;
#endif
	int curr, num_entries, still_searching;

	ctr_info(" > verbs_wait_send_buffer_rdma(%d, %d)", proc, tag);

	if( __initialized <= 0 ) {
		log_err("verbs_wait_send_buffer_rdma(): Library not initialized.	Call photon_init() first");
		return -1;
	}

	// If we've received a Rendezvous-Start from processor "proc" that is still pending
	curr_remote_buffer = verbs_processes[proc].curr_remote_buffer;
	if ( curr_remote_buffer->request != NULL_COOKIE ) {
		// If it is for the same tag, return without looking
		if ( curr_remote_buffer->tag == tag ) {
			goto normal_exit;
		}
		else {   // Otherwise it's an error.	We should never process a Rendezvous-Start before
			// fully serving the previous ones.
			goto error_exit;
		}
	}

	curr = verbs_processes[proc].local_snd_info_ledger->curr;
	curr_entry = &(verbs_processes[proc].local_snd_info_ledger->entries[curr]);

	dbg_info("verbs_wait_send_buffer_rdma(): Spinning on info ledger looking for receive request");
	dbg_info("verbs_wait_send_buffer_rdma(): looking in position %d/%p", curr, curr_entry);

#ifdef DEBUG
	stime = time(NULL);
#endif
	count = 1;
	still_searching = 1;
	entry_iterator = curr_entry;
	do {
		while(entry_iterator->header == 0 || entry_iterator->footer == 0) {
#ifdef DEBUG
			stime = _tictoc(stime, proc);
#else
			;
#endif
		}
		if( (tag < 0) || (entry_iterator->tag == tag ) ) {
			still_searching = 0;
		}
		else {
			curr = (verbs_processes[proc].local_snd_info_ledger->curr + count++) % verbs_processes[proc].local_snd_info_ledger->num_entries;
			entry_iterator = &(verbs_processes[proc].local_snd_info_ledger->entries[curr]);
		}
	}
	while(still_searching);

	// If it wasn't the first pending receive request, swap the one we will serve (entry_iterator) with
	// the first pending (curr_entry) in the info ledger, so that we can increment the current pointer
	// (verbs_processes[proc].local_snd_info_ledger->curr) and skip the request we will serve without losing any
	// pending requests.
	if( entry_iterator != curr_entry ) {
		tmp_entry = *entry_iterator;
		*entry_iterator = *curr_entry;
		*curr_entry = tmp_entry;
	}

	verbs_processes[proc].curr_remote_buffer->request = curr_entry->request;
	verbs_processes[proc].curr_remote_buffer->rkey = curr_entry->rkey;
	verbs_processes[proc].curr_remote_buffer->addr = curr_entry->addr;
	verbs_processes[proc].curr_remote_buffer->size = curr_entry->size;
	verbs_processes[proc].curr_remote_buffer->tag = curr_entry->tag;

	dbg_info("verbs_wait_send_buffer_rdma(): Request: %u", curr_entry->request);
	dbg_info("verbs_wait_send_buffer_rdma(): Context: %u", curr_entry->rkey);
	dbg_info("verbs_wait_send_buffer_rdma(): Address: %p", (void *)curr_entry->addr);
	dbg_info("verbs_wait_send_buffer_rdma(): Size: %u", curr_entry->size);
	dbg_info("verbs_wait_send_buffer_rdma(): Tag: %d", curr_entry->tag);

	curr_entry->header = 0;
	curr_entry->footer = 0;

	num_entries = verbs_processes[proc].local_snd_info_ledger->num_entries;
	curr = verbs_processes[proc].local_snd_info_ledger->curr;
	curr = (curr + 1) % num_entries;
	verbs_processes[proc].local_snd_info_ledger->curr = curr;

	dbg_info("verbs_wait_send_buffer_rdma(): new curr == %d", verbs_processes[proc].local_snd_info_ledger->curr);

normal_exit:
	return 0;
error_exit:
	return 1;

}


////////////////////////////////////////////////////////////////////////////////
//// verbs_wait_send_request_rdma() treats "tag == -1" as ANY_TAG
int verbs_wait_send_request_rdma(int tag) {
	verbs_ri_ledger_entry_t *curr_entry, *entry_iterator, tmp_entry;
	int count, iproc;
#ifdef DEBUG
	time_t stime;
#endif
	int curr, num_entries, still_searching;

	ctr_info(" > verbs_wait_send_request_rdma(%d)", tag);

	if( __initialized <= 0 ) {
		log_err("verbs_wait_send_request_rdma(): Library not initialized.	 Call photon_init() first");
		goto error_exit;
	}

	dbg_info("verbs_wait_send_request_rdma(): Spinning on send info ledger looking for send request");

	still_searching = 1;
	iproc = -1;
#ifdef DEBUG
	stime = time(NULL);
#endif
	do {
		iproc = (iproc+1)%_photon_nproc;
		curr = verbs_processes[iproc].local_snd_info_ledger->curr;
		curr_entry = &(verbs_processes[iproc].local_snd_info_ledger->entries[curr]);
		dbg_info("verbs_wait_send_request_rdma(): looking in position %d/%p for proc %d", curr, curr_entry,iproc);

		count = 1;
		entry_iterator = curr_entry;
		// Some peers (procs) might have sent more than one send requests using different tags, so check them all.
		while(entry_iterator->header == 1 && entry_iterator->footer == 1) {
			if( (entry_iterator->addr == (uintptr_t)0) && (entry_iterator->rkey == 0) && ((tag < 0) || (entry_iterator->tag == tag )) ) {
				still_searching = 0;
				dbg_info("verbs_wait_send_request_rdma(): Found matching send request with tag %d from proc %d", tag, iproc);
				break;
			}
			else {
				dbg_info("verbs_wait_send_request_rdma(): Found non-matching send request with tag %d from proc %d", tag, iproc);
				curr = (verbs_processes[iproc].local_snd_info_ledger->curr + count) % verbs_processes[iproc].local_snd_info_ledger->num_entries;
				++count;
				entry_iterator = &(verbs_processes[iproc].local_snd_info_ledger->entries[curr]);
			}
		}
#ifdef DEBUG
		stime = _tictoc(stime, -1);
#endif
	}
	while(still_searching);

	// If it wasn't the first pending send request, swap the one we will serve (entry_iterator) with
	// the first pending (curr_entry) in the send info ledger, so that we can increment the current pointer
	// (verbs_processes[iproc].local_snd_info_ledger->curr) and skip the request we will serve without losing any
	// pending requests.
	if( entry_iterator != curr_entry ) {
		tmp_entry = *entry_iterator;
		*entry_iterator = *curr_entry;
		*curr_entry = tmp_entry;
	}

	curr_entry->header = 0;
	curr_entry->footer = 0;
	// NOTE:
	// curr_entry->request contains the curr_cookie_count om the sender size.	 In the current implementation we
	// are not doing anything with it.	Maybe we should keep it somehow and pass it back to the sender with
	// through post_recv_buffer().

	num_entries = verbs_processes[iproc].local_snd_info_ledger->num_entries;
	curr = verbs_processes[iproc].local_snd_info_ledger->curr;
	curr = (curr + 1) % num_entries;
	verbs_processes[iproc].local_snd_info_ledger->curr = curr;

	dbg_info("verbs_wait_send_request_rdma(): new curr == %d", verbs_processes[iproc].local_snd_info_ledger->curr);

	return iproc;

error_exit:
	return -1;

}


///////////////////////////////////////////////////////////////////////////////
int verbs_post_recv_buffer_rdma(int proc, char *ptr, uint32_t size, int tag, uint32_t *request) {
	verbs_buffer_t *db;
	uint64_t cookie;
	verbs_ri_ledger_entry_t *entry;
	int curr, num_entries, qp_index, err;
	uint32_t request_id;

	ctr_info(" > verbs_post_recv_buffer_rdma(%d, %p, %u, %d, %p)", proc, ptr, size, tag, request);

	if( __initialized <= 0 ) {
		log_err("verbs_post_recv_buffer_rdma(): Library not initialized.	Call photon_init() first");
		return -1;
	}

	if (buffertable_find_containing( (void *) ptr, (int)size, &db) != 0) {
		log_err("verbs_post_recv_buffer_rdma(): Requested recv from ptr not in table");
		goto error_exit;
	}

	request_id = INC_COUNTER(curr_cookie);
	dbg_info("Incrementing curr_cookie_count to: %d", request_id);

	// proc == -1 means ANY_SOURCE.	 In this case all potential senders must post a send request
	// which will write into our snd_info ledger entries such that:
	// rkey == 0
	// addr == (uintptr_t)0
	if( proc == -1 ) {
		proc = verbs_wait_send_request_rdma(tag);
	}

	curr = verbs_processes[proc].remote_rcv_info_ledger->curr;
	entry = &verbs_processes[proc].remote_rcv_info_ledger->entries[curr];

	// fill in what we're going to transfer
	entry->header = 1;
	entry->request = request_id;
	entry->rkey = db->mr->rkey;
	entry->addr = (uintptr_t) ptr;
	entry->size = size;
	entry->tag = tag;
	entry->footer = 1;

	dbg_info("Post recv");
	dbg_info("Request: %u", entry->request);
	dbg_info("rkey: %u", entry->rkey);
	dbg_info("Address: %p", (void *)entry->addr);
	dbg_info("Size: %u", entry->size);
	dbg_info("Tag: %d", entry->tag);

	{
		uintptr_t rmt_addr;

		struct ibv_sge list = {
			.addr		= (uintptr_t)entry,
			.length = sizeof(*entry),
			.lkey		= shared_storage->mr->lkey
		};

		rmt_addr	= verbs_processes[proc].remote_rcv_info_ledger->remote.addr;
		rmt_addr += verbs_processes[proc].remote_rcv_info_ledger->curr * sizeof(*entry);
		cookie = (( (uint64_t)proc)<<32) | NULL_COOKIE;
		struct ibv_send_wr wr = {
			.wr_id			= cookie,
			.sg_list		= &list,
			.num_sge		= 1,
			.opcode			= IBV_WR_RDMA_WRITE,
			.send_flags = IBV_SEND_SIGNALED,
			.wr.rdma.remote_addr = rmt_addr,
			.wr.rdma.rkey = verbs_processes[proc].remote_rcv_info_ledger->remote.rkey
		};
		struct ibv_send_wr *bad_wr;

		qp_index = 0; // TODO: make that something like (++qp_index)%num_qp

		do {
			err = ibv_post_send(verbs_processes[proc].qp[qp_index], &wr, &bad_wr);
#ifndef PHOTON_MULTITHREADED
			if( err && __verbs_wait_one() ) {
				log_err("verbs_post_recv_buffer_rdma(): ibv_post_send() returned: %d and __verbs_wait_one() failed. Exiting",err);
				goto error_exit;
			}
#endif
		}
		while( err );
	}

	// TODO: I don't think that this is a sufficient solution.
	// Keep track of the dat_ep_post_rdma_write()s that do not insert an entry into "reqtable", so
	// that __verbs_wait_one() is able to wait on this event's completion
	INC_COUNTER(handshake_rdma_write);

	if (request != NULL) {
		verbs_req_t *req;

		req = verbs_get_request();
		if (!req) {
			log_err("verbs_post_recv(): Couldn't allocate request\n");
			goto error_exit;
		}
		req->id = request_id;
		req->state = REQUEST_PENDING;
		// verbs_post_recv_buffer_rdma() initiates a receiver initiated handshake.	For this reason,
		// we don't care when the function is completed, but rather when the transfer associated with
		// this handshake is completed.	 This will be reflected in the LEDGER by the corresponding
		// verbs_send_FIN() posted by the sender.
		req->type = LEDGER;
		req->proc = proc;
		req->tag = tag;
		SAFE_LIST_INSERT_HEAD(&pending_reqs_list, req, list);

		dbg_info("Inserting the RDMA request into the request table: %d/%p", request_id, req);

		if (htable_insert(ledger_reqtable, (uint64_t)request_id, req) != 0) {
			// this is bad, we've submitted the request, but we can't track it
			log_err("Couldn't save request in hashtable");
		}
		*request = request_id;
	}

	num_entries = verbs_processes[proc].remote_rcv_info_ledger->num_entries;
	curr = verbs_processes[proc].remote_rcv_info_ledger->curr;
	curr = (curr + 1) % num_entries;
	verbs_processes[proc].remote_rcv_info_ledger->curr = curr;

	dbg_info("verbs_post_recv_buffer_rdma(): New curr (proc=%d): %u", proc, verbs_processes[proc].remote_rcv_info_ledger->curr);

	return 0;

error_exit:
	if (request != NULL) {
		*request = NULL_COOKIE;
	}
	return -1;
}


///////////////////////////////////////////////////////////////////////////////
int verbs_post_send_request_rdma(int proc, uint32_t size, int tag, uint32_t *request) {
	verbs_ri_ledger_entry_t *entry;
	int curr, num_entries, qp_index, err;
	uint64_t cookie;
	uint32_t request_id;

	ctr_info(" > verbs_post_send_request_rdma(%d, %u, %d, %p)", proc, size, tag, request);

	if( __initialized <= 0 ) {
		log_err("verbs_post_send_request_rdma(): Library not initialized.	 Call photon_init() first");
		goto error_exit;
	}

	request_id = INC_COUNTER(curr_cookie);
	dbg_info("Incrementing curr_cookie_count to: %d", request_id);

	curr = verbs_processes[proc].remote_snd_info_ledger->curr;
	entry = &verbs_processes[proc].remote_snd_info_ledger->entries[curr];

	// fill in what we're going to transfer
	entry->header = 1;
	entry->request = request_id;
	entry->rkey = 0;						// We are not really giving our peer information about some
	entry->addr = (uintptr_t)0; // send buffer here.	Just our intention to send() in the future.
	entry->size = size;
	entry->tag = tag;
	entry->footer = 1;

	dbg_info("Post send request");
	dbg_info("Request: %u", entry->request);
	dbg_info("rkey: %u", entry->rkey);
	dbg_info("Addr: %p", (void *)entry->addr);
	dbg_info("Size: %u", entry->size);
	dbg_info("Tag: %d", entry->tag);

	{
		uintptr_t rmt_addr;
		struct ibv_send_wr *bad_wr;

		struct ibv_sge list = {
			.addr		= (uintptr_t)entry,
			.length = sizeof(*entry),
			.lkey		= shared_storage->mr->lkey
		};

		rmt_addr	= verbs_processes[proc].remote_snd_info_ledger->remote.addr;
		rmt_addr += verbs_processes[proc].remote_snd_info_ledger->curr * sizeof(*entry);
		cookie = (( (uint64_t)proc)<<32) | request_id;
		struct ibv_send_wr wr = {
			.wr_id			= cookie,
			.sg_list		= &list,
			.num_sge		= 1,
			.opcode			= IBV_WR_RDMA_WRITE,
			.send_flags = IBV_SEND_SIGNALED,
			.wr.rdma.remote_addr = rmt_addr,
			.wr.rdma.rkey = verbs_processes[proc].remote_snd_info_ledger->remote.rkey
		};

		qp_index = 0; // TODO: make that something like (++qp_index)%num_qp
		do {
			err = ibv_post_send(verbs_processes[proc].qp[qp_index], &wr, &bad_wr);
#ifndef PHOTON_MULTITHREADED
			if( err && __verbs_wait_one() ) {
				log_err("verbs_post_send_request_rdma(): ibv_post_send() returned: %d and __verbs_wait_one() failed. Exiting",err);
				goto error_exit;
			}
#endif
		}
		while( err );
	}

	// Keep track of the dat_ep_post_rdma_write()s that do not insert an entry into "reqtable", so
	// that __verbs_wait_one() is able to wait on this event's completion
	INC_COUNTER(handshake_rdma_write);

	if (request != NULL) {
		verbs_req_t *req;

		req = verbs_get_request();
		if (!req) {
			log_err("verbs_post_send_request_rdma(): Couldn't allocate request\n");
			goto error_exit;
		}
		req->id = request_id;
		req->state = REQUEST_PENDING;
		// verbs_post_send_request_rdma() causes an RDMA transfer, but its own completion is
		// communicated to the task that posts it through a DTO completion event.	 This
		// function informs the receiver about an upcoming send, it does NOT initiate
		// a data transfer handshake and that's why it's not a LEDGER event.
		req->type = EVQUEUE;
		req->proc = proc;
		req->tag = tag;
		SAFE_LIST_INSERT_HEAD(&pending_reqs_list, req, list);

		dbg_info("verbs_post_send_request_rmda(): Inserting the RDMA request into the request table: %d/%p", request_id, req);

		if (htable_insert(reqtable, (uint64_t)request_id, req) != 0) {
			// this is bad, we've submitted the request, but we can't track it
			log_err("Couldn't save request in hashtable");
		}
		*request = request_id;
	}

	num_entries = verbs_processes[proc].remote_snd_info_ledger->num_entries;
	curr = verbs_processes[proc].remote_snd_info_ledger->curr;
	curr = (curr + 1) % num_entries;
	verbs_processes[proc].remote_snd_info_ledger->curr = curr;
	dbg_info("verbs_post_send_request_rmda(): New curr: %u", curr);

	return 0;

error_exit:
	if (request != NULL) {
		*request = NULL_COOKIE;
	}
	return -1;
}


///////////////////////////////////////////////////////////////////////////////
int verbs_post_send_buffer_rdma(int proc, char *ptr, uint32_t size, int tag, uint32_t *request) {
	verbs_buffer_t *db;
	verbs_ri_ledger_entry_t *entry;
	int curr, num_entries, qp_index, err;
	uint64_t cookie;
	uint32_t request_id;

	ctr_info(" > verbs_post_send_buffer_rdma(%d, %p, %u, %d, %p)", proc, ptr, size, tag, request);

	if( __initialized <= 0 ) {
		log_err("verbs_post_send_buffer_rdma(): Library not initialized.	Call photon_init() first");
		goto error_exit;
	}

	if (buffertable_find_containing( (void *) ptr, (int)size, &db) != 0) {
		log_err("verbs_post_send_buffer_rdma(): Requested post of send buffer for ptr not in table");
		goto error_exit;
	}

	request_id = INC_COUNTER(curr_cookie);
	dbg_info("Incrementing curr_cookie_count to: %d", request_id);

	curr = verbs_processes[proc].remote_snd_info_ledger->curr;
	entry = &verbs_processes[proc].remote_snd_info_ledger->entries[curr];

	// fill in what we're going to transfer
	entry->header = 1;
	entry->request = request_id;
	entry->rkey = db->mr->rkey;
	entry->addr = (uintptr_t)ptr;
	entry->size = size;
	entry->tag = tag;
	entry->footer = 1;

	dbg_info("Post send request");
	dbg_info("Request: %u", entry->request);
	dbg_info("rkey: %u", entry->rkey);
	dbg_info("Addr: %p", (void *)entry->addr);
	dbg_info("Size: %u", entry->size);
	dbg_info("Tag: %d", entry->tag);

	{
		uintptr_t rmt_addr;
		struct ibv_send_wr *bad_wr;

		struct ibv_sge list = {
			.addr		= (uintptr_t)entry,
			.length = sizeof(*entry),
			.lkey		= shared_storage->mr->lkey
		};

		rmt_addr	= verbs_processes[proc].remote_snd_info_ledger->remote.addr;
		rmt_addr += verbs_processes[proc].remote_snd_info_ledger->curr * sizeof(*entry);
		cookie = (( (uint64_t)proc)<<32) | request_id;
		struct ibv_send_wr wr = {
			.wr_id			= cookie,
			.sg_list		= &list,
			.num_sge		= 1,
			.opcode			= IBV_WR_RDMA_WRITE,
			.send_flags = IBV_SEND_SIGNALED,
			.wr.rdma.remote_addr = rmt_addr,
			.wr.rdma.rkey = verbs_processes[proc].remote_snd_info_ledger->remote.rkey
		};

		qp_index = 0; // TODO: make that something like (++qp_index)%num_qp
		do {
			err = ibv_post_send(verbs_processes[proc].qp[qp_index], &wr, &bad_wr);
#ifndef PHOTON_MULTITHREADED
			if( err && __verbs_wait_one() ) {
				log_err("verbs_post_send_buffer_rdma(): ibv_post_send() returned: %d and __verbs_wait_one() failed. Exiting",err);
				goto error_exit;
			}
#endif
		}
		while( err );
	}

	// Keep track of the dat_ep_post_rdma_write()s that do not insert an entry into "reqtable", so
	// that __verbs_wait_one() is able to wait on this event's completion
	INC_COUNTER(handshake_rdma_write);

	if (request != NULL) {
		verbs_req_t *req;

		req = verbs_get_request();
		if (!req) {
			log_err("verbs_post_send_buffer_rdma(): Couldn't allocate request\n");
			goto error_exit;
		}
		req->id = request_id;
		req->state = REQUEST_PENDING;
		// verbs_post_send_buffer_rdma() initiates a sender initiated handshake.	For this reason,
		// we don't care when the function is completed, but rather when the transfer associated with
		// this handshake is completed.	 This will be reflected in the LEDGER by the corresponding
		// verbs_send_FIN() posted by the receiver.
		req->type = LEDGER;
		req->proc = proc;
		req->tag = tag;
		SAFE_LIST_INSERT_HEAD(&pending_reqs_list, req, list);

		dbg_info("verbs_post_send_buffer_rmda(): Inserting the RDMA request into the request table: %d/%p", request_id, req);

		if (htable_insert(ledger_reqtable, (uint64_t)request_id, req) != 0) {
			// this is bad, we've submitted the request, but we can't track it
			log_err("Couldn't save request in hashtable");
		}
		*request = request_id;
	}

	num_entries = verbs_processes[proc].remote_snd_info_ledger->num_entries;
	curr = verbs_processes[proc].remote_snd_info_ledger->curr;
	curr = (curr + 1) % num_entries;
	verbs_processes[proc].remote_snd_info_ledger->curr = curr;
	dbg_info("verbs_post_send_buffer_rmda(): New curr: %u", curr);

	return 0;

error_exit:
	if (request != NULL) {
		*request = NULL_COOKIE;
	}
	return -1;
}


///////////////////////////////////////////////////////////////////////////////
static inline verbs_req_t *verbs_get_request() {
	verbs_req_t *req;

	LIST_LOCK(&free_reqs_list);
	req = LIST_FIRST(&free_reqs_list);
	if (req)
		LIST_REMOVE(req, list);
	LIST_UNLOCK(&free_reqs_list);

	if (!req) {
		req = malloc(sizeof(verbs_req_t));
		pthread_mutex_init(&req->mtx, NULL);
		pthread_cond_init (&req->completed, NULL);
	}

	return req;
}


///////////////////////////////////////////////////////////////////////////////
int verbs_post_os_put(int proc, char *ptr, uint32_t size, int tag, uint32_t remote_offset, uint32_t *request) {
	verbs_remote_buffer_t *drb;
	verbs_buffer_t *db;
	uint64_t cookie;
	int qp_index, err;
	uint32_t request_id;

	ctr_info(" > verbs_post_os_put(%d, %p, %u, %u, %p)", proc, ptr, size, remote_offset, request);

	if( __initialized <= 0 ) {
		log_err("verbs_post_os_put(): Library not initialized.	Call photon_init() first");
		return -1;
	}

	drb = verbs_processes[proc].curr_remote_buffer;

	if (drb->request == NULL_COOKIE) {
		log_err("verbs_post_os_put(): Tried posting a send with no recv buffer. Have you called verbs_wait_recv_buffer_rdma() first?");
		return -1;
	}

	if (buffertable_find_containing( (void *)ptr, (int)size, &db) != 0) {
		log_err("verbs_post_os_put(): Tried posting a send for a buffer not registered");
		return -1;
	}

	if (drb->size > 0 && size + remote_offset > drb->size) {
		log_err("verbs_post_os_put(): Requested to send %u bytes to a %u buffer size at offset %u", size, drb->size, remote_offset);
		return -2;
	}

	request_id = INC_COUNTER(curr_cookie);
	dbg_info("Incrementing curr_cookie_count to: %d", request_id);

	cookie = (( (uint64_t)proc)<<32) | request_id;
	dbg_info("Posted Cookie: %u/%u/%"PRIo64, proc, request_id, cookie);

	{
		struct ibv_send_wr *bad_wr;

		struct ibv_sge list = {
			.addr		= (uintptr_t)ptr,
			.length = size,
			.lkey		= db->mr->lkey
		};

		struct ibv_send_wr wr = {
			.wr_id			= cookie,
			.sg_list		= &list,
			.num_sge		= 1,
			.opcode			= IBV_WR_RDMA_WRITE,
			.send_flags = IBV_SEND_SIGNALED,
			.wr.rdma.remote_addr = drb->addr + (uintptr_t)remote_offset,
			.wr.rdma.rkey = drb->rkey
		};

		qp_index = 0; // TODO: make that something like (++qp_index)%num_qp
		do {
			err = ibv_post_send(verbs_processes[proc].qp[qp_index], &wr, &bad_wr);
#ifndef PHOTON_MULTITHREADED
			if( err && __verbs_wait_one() ) {
				log_err("verbs_post_os_put(): ibv_post_send() returned: %d and __verbs_wait_one() failed. Exiting",err);
				goto error_exit;
			}
#endif
		}
		while( err );
	}


	if (request != NULL) {
		verbs_req_t *req;

		*request = request_id;

		req = verbs_get_request();
		if (!req) {
			log_err("verbs_post_os_put(): Couldn't allocate request\n");
			goto error_exit;
		}
		req->id = request_id;
		req->state = REQUEST_PENDING;
		// verbs_post_os_put() causes an RDMA transfer, but its own completion is
		// communicated to the task that posts it through a completion event.
		req->type = EVQUEUE;
		req->proc = proc;
		req->tag = tag;
		SAFE_LIST_INSERT_HEAD(&pending_reqs_list, req, list);

		dbg_info("Inserting the OS send request into the request table: %d/%d/%p", proc, request_id, req);

		if (htable_insert(reqtable, (uint64_t)request_id, req) != 0) {
			// this is bad, we've submitted the request, but we can't track it
			log_err("verbs_post_os_put(): Couldn't save request in hashtable");
		}
	}

	return 0;
error_exit:
	if (request != NULL) {
		*request = NULL_COOKIE;
	}
	return -1;
}



///////////////////////////////////////////////////////////////////////////////
int verbs_post_os_get(int proc, char *ptr, uint32_t size, int tag, uint32_t remote_offset, uint32_t *request) {
	verbs_remote_buffer_t *drb;
	verbs_buffer_t *db;
	uint64_t cookie;
	int qp_index, err;
	uint32_t request_id;

	ctr_info(" > verbs_post_os_get(%d, %p, %u, %u, %p)", proc, ptr, size, remote_offset, request);

	if( __initialized <= 0 ) {
		log_err("verbs_post_os_get(): Library not initialized.	Call photon_init() first");
		return -1;
	}

	drb = verbs_processes[proc].curr_remote_buffer;

	if (drb->request == NULL_COOKIE) {
		log_err("verbs_post_os_get(): Tried posting an os_get() with no send buffer");
		return -1;
	}

	if (buffertable_find_containing( (void *)ptr, (int)size, &db) != 0) {
		log_err("verbs_post_os_get(): Tried posting a og_get() into a buffer that's not registered");
		return -1;
	}

	if ( (drb->size > 0) && ((size+remote_offset) > drb->size) ) {
		log_err("verbs_post_os_get(): Requested to get %u bytes from a %u buffer size at offset %u", size, drb->size, remote_offset);
		return -2;
	}

	request_id = INC_COUNTER(curr_cookie);
	dbg_info("Incrementing curr_cookie_count to: %d", request_id);

	cookie = (( (uint64_t)proc)<<32) | request_id;
	dbg_info("Posted Cookie: %u/%u/%"PRIo64, proc, request_id, cookie);

	{
		struct ibv_send_wr *bad_wr;

		struct ibv_sge list = {
			.addr		= (uintptr_t)ptr,
			.length = size,
			.lkey		= db->mr->lkey
		};

		struct ibv_send_wr wr = {
			.wr_id			= cookie,
			.sg_list		= &list,
			.num_sge		= 1,
			.opcode			= IBV_WR_RDMA_READ,
			.send_flags = IBV_SEND_SIGNALED,
			.wr.rdma.remote_addr = drb->addr + (uintptr_t)remote_offset,
			.wr.rdma.rkey = drb->rkey
		};

		qp_index = 0; // TODO: make that something like (++qp_index)%num_qp
		do {
			err = ibv_post_send(verbs_processes[proc].qp[qp_index], &wr, &bad_wr);
#ifndef PHOTON_MULTITHREADED
			if( err && __verbs_wait_one() ) {
				log_err("verbs_post_os_get(): ibv_post_send() returned: %d and __verbs_wait_one() failed. Exiting",err);
				goto error_exit;
			}
#endif
		}
		while( err );
	}

	if (request != NULL) {
		verbs_req_t *req;

		*request = request_id;

		req = verbs_get_request();
		if (!req) {
			log_err("verbs_post_os_get(): Couldn't allocate request\n");
			goto error_exit;
		}
		req->id = request_id;
		req->state = REQUEST_PENDING;
		// verbs_post_os_get() causes an RDMA transfer, but its own completion is
		// communicated to the task that posts it through a DTO completion event.
		req->type = EVQUEUE;
		req->proc = proc;
		req->tag = tag;
		SAFE_LIST_INSERT_HEAD(&pending_reqs_list, req, list);

		dbg_info("Inserting the OS get request into the request table: %d/%d/%p", proc, request_id, req);

		if (htable_insert(reqtable, (uint64_t)request_id, req) != 0) {
			// this is bad, we've submitted the request, but we can't track it
			log_err("verbs_post_os_get(): Couldn't save request in hashtable");
		}
	}

	return 0;
error_exit:
	if (request != NULL) {
		*request = NULL_COOKIE;
	}
	return -1;
}



///////////////////////////////////////////////////////////////////////////////
int verbs_send_FIN(int proc) {
	verbs_remote_buffer_t *drb;
	verbs_rdma_FIN_ledger_entry_t *entry;
	int curr, num_entries, qp_index, err;

	ctr_info(" > verbs_send_FIN(%d)", proc);

	if( __initialized <= 0 ) {
		log_err("verbs_send_FIN(): Library not initialized.	 Call photon_init() first");
		return -1;
	}

	if (verbs_processes[proc].curr_remote_buffer->request == NULL_COOKIE) {
		log_err("verbs_send_FIN(): Cannot send FIN, curr_remote_buffer->request is NULL_COOKIE");
		goto error_exit;
	}

	drb = verbs_processes[proc].curr_remote_buffer;
	curr = verbs_processes[proc].remote_FIN_ledger->curr;
	entry = &verbs_processes[proc].remote_FIN_ledger->entries[curr];
	dbg_info("verbs_send_FIN() verbs_processes[%d].remote_FIN_ledger->curr==%d\n",proc, curr);

	if( entry == NULL ) {
		log_err("verbs_send_FIN() entry is NULL for proc=%d\n",proc);
		return 1;
	}

	entry->header = 1;
	entry->request = drb->request;
	entry->footer = 1;

	{
		struct ibv_send_wr *bad_wr;
		uintptr_t rmt_address;
		uint64_t cookie;

		struct ibv_sge list = {
			.addr		= (uintptr_t)entry,
			.length = sizeof(*entry),
			.lkey		= shared_storage->mr->lkey
		};

		rmt_address	 = verbs_processes[proc].remote_FIN_ledger->remote.addr;
		rmt_address += verbs_processes[proc].remote_FIN_ledger->curr * sizeof(*entry);
		cookie = (( (uint64_t)proc)<<32) | NULL_COOKIE;
		struct ibv_send_wr wr = {
			.wr_id			= cookie,
			.sg_list		= &list,
			.num_sge		= 1,
			.opcode			= IBV_WR_RDMA_WRITE,
			.send_flags = IBV_SEND_SIGNALED,
			.wr.rdma.remote_addr = rmt_address,
			.wr.rdma.rkey = verbs_processes[proc].remote_FIN_ledger->remote.rkey
		};

		qp_index = 0; // TODO: make that something like (++qp_index)%num_qp
		do {
			err = ibv_post_send(verbs_processes[proc].qp[qp_index], &wr, &bad_wr);
#ifndef PHOTON_MULTITHREADED
			if( err && __verbs_wait_one() ) {
				log_err("verbs_send_FINE(): ibv_post_send() returned: %d and __verbs_wait_one() failed. Exiting",err);
				goto error_exit;
			}
#endif
		}
		while( err );
	}

	num_entries = verbs_processes[proc].remote_FIN_ledger->num_entries;
	curr = verbs_processes[proc].remote_FIN_ledger->curr;
	curr = (curr + 1) % num_entries;
	verbs_processes[proc].remote_FIN_ledger->curr = curr;

	drb->request = NULL_COOKIE;

	return 0;
error_exit:
	return -1;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
// the actual photon API

inline int photon_init(photonConfig cfg) {
	return verbs_init(cfg);
}

inline int photon_register_buffer(char *buffer, int buffer_size) {
	return verbs_register_buffer(buffer, buffer_size);
}

inline int photon_unregister_buffer(char *buffer, int size) {
	return verbs_unregister_buffer(buffer, size);
}

/*
inline int photon_post_recv(int proc, char *ptr, uint32_t size, uint32_t *request) {
		return verbs_post_recv(proc, ptr, size, request);
}

inline int photon_post_send(int proc, char *ptr, uint32_t size, uint32_t *request) {
		return verbs_post_send(proc, ptr, size, request);
}
*/

inline int photon_test(uint32_t request, int *flag, int *type, MPI_Status *status) {
	return verbs_test(request, flag, type, status);
}

inline int photon_wait(uint32_t request) {
	return verbs_wait(request);
}

inline int photon_wait_ledger(uint32_t request) {
	return verbs_wait(request);
}

/*
inline int photon_wait_remaining() {
		return verbs_wait_remaining();
}

inline int photon_wait_remaining_ledger() {
		return verbs_wait_remaining_ledger();
}
*/

inline int photon_post_recv_buffer_rdma(int proc, char *ptr, uint32_t size, int tag, uint32_t *request) {
	return verbs_post_recv_buffer_rdma(proc, ptr, size, tag, request);
}

inline int photon_post_send_buffer_rdma(int proc, char *ptr, uint32_t size, int tag, uint32_t *request) {
	return verbs_post_send_buffer_rdma(proc, ptr, size, tag, request);
}

inline int photon_post_send_request_rdma(int proc, uint32_t size, int tag, uint32_t *request) {
	return verbs_post_send_request_rdma(proc, size, tag, request);
}

inline int photon_wait_recv_buffer_rdma(int proc, int tag) {
	return verbs_wait_recv_buffer_rdma(proc, tag);
}

inline int photon_wait_send_buffer_rdma(int proc, int tag) {
	return verbs_wait_send_buffer_rdma(proc, tag);
}

inline int photon_wait_send_request_rdma(int tag) {
	return verbs_wait_send_request_rdma(tag);
}

inline int photon_post_os_put(int proc, char *ptr, uint32_t size, int tag, uint32_t remote_offset, uint32_t *request) {
	return verbs_post_os_put(proc, ptr, size, tag, remote_offset, request);
}

inline int photon_post_os_get(int proc, char *ptr, uint32_t size, int tag, uint32_t remote_offset, uint32_t *request) {
	return verbs_post_os_get(proc, ptr, size, tag, remote_offset, request);
}

inline int photon_send_FIN(int proc) {
	return verbs_send_FIN(proc);
}

inline int photon_wait_any(int *ret_proc, uint32_t *ret_req) {
#ifdef PHOTON_MULTITHREADED
	// TODO: These can probably be implemented by having
	//	 a condition var for the unreaped lists
	return -1;
#else
	return verbs_wait_any(ret_proc, ret_req);
#endif
}

inline int photon_wait_any_ledger(int *ret_proc, uint32_t *ret_req) {
#ifdef PHOTON_MULTITHREADED
	return -1;
#else
	return verbs_wait_any_ledger(ret_proc, ret_req);
#endif
}

inline int photon_finalize() {
	return verbs_finalize();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
//																		 DAPL Utility Functions																		 //
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////


static int verbs_init_context(ProcessInfo *verbs_processes) {

	struct ibv_device **dev_list;
	int i, iproc, num_qp, num_devs;

	ctr_info(" > verbs_init_context()");

	// FIXME: Are we using random numbers anywhere?
	srand48(getpid() * time(NULL));

	dev_list = ibv_get_device_list(&num_devs);
	if (!dev_list || !dev_list[0]) {
		fprintf(stderr, "No IB devices found\n");
		return 1;
	}

	for (i=0; i<=num_devs; i++) {
		if (!strcmp(ibv_get_device_name(dev_list[i]), phot_verbs_ib_dev)) {
			ctr_info(" > verbs_init_context(): using device %s:%d", ibv_get_device_name(dev_list[i]), phot_verbs_ib_port);
			break;
		}
	}

	phot_verbs_context = ibv_open_device(dev_list[i]);
	if (!phot_verbs_context) {
		fprintf(stderr, "Couldn't get context for %s\n", ibv_get_device_name(dev_list[i]));
		return 1;
	}
	ctr_info(" > verbs_init_context(): context has device %s", ibv_get_device_name(phot_verbs_context->device));

	phot_verbs_pd = ibv_alloc_pd(phot_verbs_context);
	if (!phot_verbs_pd) {
		fprintf(stderr, "Couldn't allocate PD\n");
		return 1;
	}

	{
		struct ibv_port_attr attr;

		memset(&attr, 0, sizeof(attr));
		if( ibv_query_port(phot_verbs_context, phot_verbs_ib_port, &attr) ) {
			fprintf(stderr, "Cannot query port");
			return 1;
		}
		phot_verbs_lid = attr.lid;
	}

	// The second argument (cq_size) can be something like 40K.	 It should be
	// within NIC MaxCQEntries limit
	phot_verbs_cq = ibv_create_cq(phot_verbs_context, 1000, NULL, NULL, 0);
	if (!phot_verbs_cq) {
		fprintf(stderr, "Couldn't create CQ\n");
		return 1;
	}

	{
		struct ibv_srq_init_attr attr = {
			.attr = {
				.max_wr	 = 500,
				.max_sge = 1
			}
		};

		phot_verbs_srq = ibv_create_srq(phot_verbs_pd, &attr);
		if (!phot_verbs_srq)	{
			fprintf(stderr, "Couldn't create SRQ\n");
			return 1;
		}
	}

	num_qp = MAX_QP;
	for (iproc = 0; iproc < _photon_nproc+_photon_forwarder; ++iproc) {

		//FIXME: What if I want to send to myself?
		if( iproc == _photon_myrank ) {
			continue;
		}

		verbs_processes[iproc].num_qp = num_qp;
		for (i = 0; i < num_qp; ++i) {
			struct ibv_qp_init_attr attr = {
				.send_cq = phot_verbs_cq,
				.recv_cq = phot_verbs_cq,
				.srq		 = phot_verbs_srq,
				.cap		 = {
					.max_send_wr	= 14,
					.max_send_sge = 1, // scatter gather element
					.max_recv_wr	= 18,
					.max_recv_sge = 1, // scatter gather element
				},
				.qp_type = IBV_QPT_RC
			};

			verbs_processes[iproc].qp[i] = ibv_create_qp(phot_verbs_pd, &attr);
			if (!verbs_processes[iproc].qp[i] ) {
				fprintf(stderr, "Couldn't create QP[%d] for task:%d\n", i, iproc);
				return 1;
			}
		}

		for (i = 0; i < num_qp; ++i) {
			struct ibv_qp_attr attr;

			attr.qp_state    = IBV_QPS_INIT;
			attr.pkey_index	 = 0;
			attr.port_num	 = phot_verbs_ib_port;
			attr.qp_access_flags = IBV_ACCESS_REMOTE_WRITE|IBV_ACCESS_REMOTE_READ;

			if (ibv_modify_qp(verbs_processes[iproc].qp[i], &attr,
			                  IBV_QP_STATE							|
			                  IBV_QP_PKEY_INDEX					|
			                  IBV_QP_PORT								|
			                  IBV_QP_ACCESS_FLAGS)) {
				fprintf(stderr, "Failed to modify QP[%d] for task:%d to INIT\n", i, iproc);
				return 1;
			}
		}
	}

	return 0;
}


static int verbs_connect_peers(ProcessInfo *verbs_processes) {
	struct verbs_cnct_info **local_info, **remote_info;
	int i, iproc, num_qp;

	ctr_info(" > verbs_connect_peers()");

	local_info	= (struct verbs_cnct_info **)malloc( _photon_nproc*sizeof(struct verbs_cnct_info *) );
	if( !local_info ) {
		goto error_exit;
	}

	num_qp = MAX_QP;
	for(iproc=0; iproc<_photon_nproc; ++iproc) {

		if( iproc == _photon_myrank ) {
			continue;
		}

		local_info[iproc]	 = (struct verbs_cnct_info *)malloc( num_qp*sizeof(struct verbs_cnct_info) );
		if( !local_info[iproc] ) {
			goto error_exit;
		}

		for(i=0; i<num_qp; ++i) {
			local_info[iproc][i].lid = phot_verbs_lid;
			local_info[iproc][i].qpn = verbs_processes[iproc].qp[i]->qp_num;
			local_info[iproc][i].psn = (lrand48() & 0xfff000) + _photon_myrank+1;
		}
	}

	remote_info = exch_cnct_info(num_qp, local_info);
	if( !remote_info ) {
		log_err("verbs_connect_peers(): Cannot exchange connect info");
		goto error_exit;
	}
	MPI_Barrier(_photon_comm);

	for (iproc = 0; iproc < _photon_nproc; ++iproc) {
		if( iproc == _photon_myrank ) {
			continue;
		}

		if( verbs_connect_qps(num_qp, local_info[iproc], remote_info[iproc], &verbs_processes[iproc]) ) {
			log_err("verbs_connect_peers(): Cannot connect queue pairs");
			goto error_exit;
		}
	}

	return 0;

error_exit:
	return -1;
}


static int verbs_connect_qps(int num_qp, struct verbs_cnct_info *local_info, struct verbs_cnct_info *remote_info, ProcessInfo *verbs_processes) {
	int i;
	int err;

	for (i = 0; i < num_qp; ++i) {
		fprintf(stderr,"[%d/%d], i=%d lid=%x qpn=%x, psn=%x, qp[i].qpn=%x\n",
		        _photon_myrank, _photon_nproc, i,
		        remote_info[i].lid, remote_info[i].qpn, remote_info[i].psn,
		        verbs_processes->qp[i]->qp_num);

		struct ibv_qp_attr attr = {
			.qp_state	    = IBV_QPS_RTR,
			.path_mtu	    = 3, // (3 == IBV_MTU_1024) which means 1024. Is this a good value?
			.dest_qp_num	    = remote_info[i].qpn,
			.rq_psn		    = remote_info[i].psn,
			.max_dest_rd_atomic = 1,
			.min_rnr_timer	    = 12,
			.ah_attr = {
				.is_global     = 0,
				.dlid	       = remote_info[i].lid,
				.sl	       = 0,
				.src_path_bits = 0,
				.port_num      = phot_verbs_ib_port
			}
		};
		err=ibv_modify_qp(verbs_processes->qp[i], &attr,
		                  IBV_QP_STATE							|
		                  IBV_QP_AV							|
		                  IBV_QP_PATH_MTU						|
		                  IBV_QP_DEST_QPN						|
		                  IBV_QP_RQ_PSN							|
		                  IBV_QP_MAX_DEST_RD_ATOMIC |
		                  IBV_QP_MIN_RNR_TIMER);
		if (err) {
			fprintf(stderr, "Failed to modify QP[%d] to RTR. Reason:%d\n", i,err);
			return 1;
		}

		attr.qp_state				= IBV_QPS_RTS;
		attr.timeout				= 14;
		attr.retry_cnt			= 7;
		attr.rnr_retry			= 7;
		attr.sq_psn					= local_info[i].psn;
		attr.max_rd_atomic	= 1;
		err=ibv_modify_qp(verbs_processes->qp[i], &attr,
		                  IBV_QP_STATE							|
		                  IBV_QP_TIMEOUT						|
		                  IBV_QP_RETRY_CNT					|
		                  IBV_QP_RNR_RETRY					|
		                  IBV_QP_SQ_PSN							|
		                  IBV_QP_MAX_QP_RD_ATOMIC);
		if (err) {
			fprintf(stderr, "Failed to modify QP[%d] to RTS. Reason:%d\n", i,err);
			return 1;
		}
	}

	return 0;
}


static struct verbs_cnct_info **exch_cnct_info(int num_qp, struct verbs_cnct_info **local_info) {
	MPI_Request *rreq;
	int peer;
	char smsg[ sizeof "00000000:00000000:00000000"];
	char **rmsg;
	int i, j, iproc;
	struct verbs_cnct_info **remote_info = NULL;
	int msg_size = sizeof "00000000:00000000:00000000";

	remote_info = (struct verbs_cnct_info **)malloc( _photon_nproc * sizeof(struct verbs_cnct_info *) );
	for (iproc = 0; iproc < _photon_nproc; ++iproc) {
		if( iproc == _photon_myrank ) {
			continue;
		}
		remote_info[iproc] = (struct verbs_cnct_info *)malloc( num_qp * sizeof(struct verbs_cnct_info) );
		if (!remote_info) {
			for (j = 0; j < iproc; j++) {
				free(remote_info[j]);
			}
			free(remote_info);
			goto err_exit;
		}
	}

	rreq = (MPI_Request *)malloc( _photon_nproc * sizeof(MPI_Request) );
	if( !rreq ) goto err_exit;

	rmsg = (char **)malloc( _photon_nproc * sizeof(char *) );
	if( !rmsg ) goto err_exit;
	for (iproc = 0; iproc < _photon_nproc; ++iproc) {
		rmsg[iproc] = (char *)malloc( msg_size );
		if( !rmsg[iproc] ) {
			int j = 0;
			for (j = 0; j < iproc; j++) {
				free(rmsg[j]);
			}
			free(rmsg);
			goto err_exit;
		}
	}

	for (i = 0; i < num_qp; ++i) {
		for (iproc=0; iproc < _photon_nproc; iproc++) {
			peer = (_photon_myrank+iproc)%_photon_nproc;
			if( peer == _photon_myrank ) {
				continue;
			}
			MPI_Irecv(rmsg[peer], msg_size, MPI_BYTE, peer, 0, _photon_comm, &rreq[peer]);

		}

		for (iproc=0; iproc < _photon_nproc; iproc++) {
			peer = (_photon_nproc+_photon_myrank-iproc)%_photon_nproc;
			if( peer == _photon_myrank ) {
				continue;
			}
			sprintf(smsg, "%08x:%08x:%08x", local_info[peer][i].lid, local_info[peer][i].qpn,
			        local_info[peer][i].psn);
			//fprintf(stderr,"[%d/%d] Sending lid:qpn:psn = %s to task=%d\n",_photon_myrank, _photon_nproc, smsg, peer);
			if( MPI_Send(smsg, msg_size , MPI_BYTE, peer, 0, _photon_comm ) != MPI_SUCCESS ) {
				fprintf(stderr, "Couldn't send local address\n");
				goto err_exit;
			}
		}

		for (iproc=0; iproc < _photon_nproc; iproc++) {
			peer = (_photon_myrank+iproc)%_photon_nproc;
			if( peer == _photon_myrank ) {
				continue;
			}

			if( MPI_Wait(&rreq[peer], MPI_STATUS_IGNORE) ) {
				fprintf(stderr, "Couldn't wait() to receive remote address\n");
				goto err_exit;
			}
			sscanf(rmsg[peer], "%x:%x:%x",
			       &remote_info[peer][i].lid, &remote_info[peer][i].qpn, &remote_info[peer][i].psn);
			//fprintf(stderr,"[%d/%d] Received lid:qpn:psn = %x:%x:%x from task=%d\n",
			//								_photon_myrank, _photon_nproc,
			//								remote_info[peer][i].lid, remote_info[peer][i].qpn, remote_info[peer][i].psn, peer);
		}
	}

	for (i = 0; i < _photon_nproc; i++) {
		free(rmsg[i]);
	}
	free(rmsg);

	return remote_info;
err_exit:
	return NULL;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
//																		 XSP-specific Functions																		 //
//	TODO: separate I/O from XSP and move functions to a different file.													 //
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef WITH_XSP

int verbs_xsp_init() {
	char *forwarder_node;

	ctr_info(" > verbs_xsp_init()");

	forwarder_node = getenv("PHOTON_FORWARDER");

	if (!forwarder_node) {
		log_err("verbs_xsp_init(): Error: no photon forwarder specified: set the environmental variable PHOTON_FORWARDER");
		return -1;
	}

	if (verbs_xsp_setup_session(&(verbs_processes[_photon_fp].sess), forwarder_node) != 0) {
		log_err("verbs_xsp_init(): Error: could not setup XSP session");
		return -1;
	}

	if (verbs_xsp_connect_phorwarder() != 0) {
		log_err("verbs_xsp_init(); couldn't setup listeners");
		return -1;
	}

	if (verbs_xsp_exchange_ri_ledgers() != 0) {
		log_err("verbs_xsp_init(); couldn't exchange rdma ledgers");
		return -1;
	}

	if (verbs_xsp_exchange_FIN_ledger() != 0) {
		log_err("verbs_xsp_init(); couldn't exchange send ledgers");
		return -1;
	}

	return 0;
}

int verbs_xsp_setup_session(libxspSess **sess, char *xsp_hop) {

	if (libxsp_init() < 0) {
		perror("libxsp_init(): failed");
		return -1;
	}

	*sess = xsp_session();
	if (!sess) {
		perror("xsp_session() failed");
		return -1;
	}

	xsp_sess_appendchild(*sess, xsp_hop, XSP_HOP_NATIVE);

	if (xsp_connect(*sess)) {
		perror("xsp_connect(): connect failed");
		return -1;
	}

	dbg_info("XSP session established with %s", xsp_hop);

	return 0;
}

int verbs_xsp_connect_phorwarder() {
	int i;
	int num_qp;
	int ci_size;
	int rmsg_type;
	int rmsg_size;
	struct verbs_cnct_info *local_info;
	struct verbs_cnct_info *remote_info;

	num_qp = MAX_QP;

	ci_size = num_qp*sizeof(struct verbs_cnct_info);
	local_info = (struct verbs_cnct_info *)malloc(ci_size);
	if( !local_info ) {
		goto error_exit;
	}

	for(i=0; i<num_qp; ++i) {
		local_info[i].lid = phot_verbs_lid;
		local_info[i].qpn = verbs_processes[_photon_fp].qp[i]->qp_num;
		local_info[i].psn = (lrand48() & 0xfff000) + _photon_myrank+1;
	}

	if (xsp_send_msg(verbs_processes[_photon_fp].sess, local_info, ci_size, PHOTON_CI) <= 0) {
		log_err("verbs_xsp_connect_phorwarder(): Couldn't send connect info");
		goto error_send;
	}

	if (xsp_recv_msg(verbs_processes[_photon_fp].sess, (void**)&remote_info, &rmsg_size, &rmsg_type) <= 0) {
		log_err("verbs_xsp_connect_phorwarder(): Couldn't receive connect info");
		goto error_recv;
	}

	if (rmsg_type != PHOTON_CI) {
		log_err("verbs_xsp_connect_phorwarder(): Received message other than PHOTON_CI");
		goto error_recv;
	}

	if (rmsg_size != ci_size) {
		log_err("verbs_xsp_connect_phorwarder(): Bad received message size: %d", rmsg_size);
		goto error_recv;
	}

	if( verbs_connect_qps(num_qp, local_info, remote_info, &verbs_processes[_photon_fp]) ) {
		log_err("verbs_xsp_connect_phorwarder(): Cannot connect queue pairs");
		goto error_qps;
	}

	return 0;

error_qps:
error_recv:
	free(remote_info);
error_send:
	free(local_info);
error_exit:
	return -1;
}

int verbs_xsp_exchange_ri_ledgers() {
	PhotonLedgerInfo li;
	PhotonLedgerInfo *ret_li;
	int ret_len;
	int ret_type;

	ctr_info(" > verbs_xsp_exchange_ri_ledgers()");

	if( __initialized != -1 ) {
		log_err("verbs_xsp_exchange_ri_ledgers(): Library not initialized.	Call photon_xsp_init() first");
		return -1;
	}

	li.rkey = shared_storage->mr->rkey;
	li.va = (uintptr_t)(verbs_processes[_photon_fp].local_rcv_info_ledger->entries);

	dbg_info("Transmitting rcv_info ledger info to phorwarder: %"PRIxPTR, li.va);

	if (xsp_send_msg(verbs_processes[_photon_fp].sess, &li, sizeof(PhotonLedgerInfo), PHOTON_RI) <= 0) {
		log_err("verbs_xsp_exchange_ri_ledgers(): Couldn't send ledger receive-info");
		goto error_exit;
	}

	if (xsp_recv_msg(verbs_processes[_photon_fp].sess, (void**)&ret_li, &ret_len, &ret_type) <= 0) {
		log_err("verbs_xsp_exchange_ri_ledgers(): Couldn't receive ledger receive-info");
		goto error_exit;
	}

	// snd_info and rcv_info ledgers are all stored in the same
	// contiguous memory region and share a common "rkey"
	// but we send everything again to make it easier for xsp
	verbs_processes[_photon_fp].remote_rcv_info_ledger->remote.rkey = ret_li->rkey;
	verbs_processes[_photon_fp].remote_rcv_info_ledger->remote.addr = ret_li->va;

	free(ret_li);

	// Send the send-info ledger pointers
	li.rkey = shared_storage->mr->rkey;
	li.va = (uintptr_t)(verbs_processes[_photon_fp].local_snd_info_ledger->entries);

	dbg_info("Transmitting snd_info ledger info to phorwarder: %"PRIxPTR, li.va);

	if (xsp_send_msg(verbs_processes[_photon_fp].sess, &li, sizeof(PhotonLedgerInfo), PHOTON_SI) <= 0) {
		log_err("verbs_xsp_exchange_ri_ledgers(): Couldn't send ledger sender-info");
		goto error_exit;
	}

	if (xsp_recv_msg(verbs_processes[_photon_fp].sess, (void**)&ret_li, &ret_len, &ret_type) <= 0) {
		log_err("verbs_xsp_exchange_ri_ledgers(): Couldn't receive ledger receive-info");
		goto error_exit;
	}

	verbs_processes[_photon_fp].remote_snd_info_ledger->remote.rkey = ret_li->rkey;
	verbs_processes[_photon_fp].remote_snd_info_ledger->remote.addr = ret_li->va;

	free(ret_li);

	return 0;

error_exit:
	return -1;
}

int verbs_xsp_exchange_FIN_ledger() {
	PhotonLedgerInfo fi;
	PhotonLedgerInfo *ret_fi;
	int ret_len;
	int ret_type;

	ctr_info(" > verbs_xsp_exchange_FIN_ledger()");

	if( __initialized != -1 ) {
		log_err("verbs_xsp_exchange_FIN_ledger(): Library not initialized.	Call photon_xsp_init() first");
		return -1;
	}

	fi.rkey = shared_storage->mr->rkey;
	fi.va = (uintptr_t)(verbs_processes[_photon_fp].local_FIN_ledger->entries);

	if (xsp_send_msg(verbs_processes[_photon_fp].sess, &fi, sizeof(PhotonLedgerInfo), PHOTON_FI) <= 0) {
		log_err("verbs_xsp_exchange_ri_ledgers(): Couldn't send ledger fin-info");
		goto error_exit;
	}

	if (xsp_recv_msg(verbs_processes[_photon_fp].sess, (void**)&ret_fi, &ret_len, &ret_type) <= 0) {
		log_err("verbs_xsp_exchange_ri_ledgers(): Couldn't receive ledger fin-info");
		goto error_exit;
	}

	// snd_info and rcv_info ledgers are all stored in the same
	// contiguous memory region and share a common "rkey"
	verbs_processes[_photon_fp].remote_FIN_ledger->remote.rkey = ret_fi->rkey;
	verbs_processes[_photon_fp].remote_FIN_ledger->remote.addr = ret_fi->va;

	free(ret_fi);

	return 0;

error_exit:
	return -1;
}

// this call sets up the context for nproc photon-xsp connections
int verbs_xsp_init_server(int nproc) {

	_photon_fp = nproc;
	if (verbs_init_common(nproc+1, _photon_fp, MPI_COMM_SELF, 0) != 0) {
		log_err("verbs_xsp_init_server(): Couldn't initialize libphoton");
		goto error_exit;
	}

	__initialized = 1;

	return 0;

error_exit:
	return -1;
}

int verbs_xsp_lookup_proc(libxspSess *sess, int *index) {
	int i;

	for(i = 0; i < _photon_nproc; i++) {
		if (verbs_processes[i].sess &&
		        !xsp_sesscmp(verbs_processes[i].sess, sess)) {
			*index = i;
			return i;
		}
	}

	*index = -1;
	return -1;
}

int photon_decode_MPI_Datatype(MPI_Datatype type, PhotonMPIDatatype *ptype) {
	int i;
	MPI_Datatype *types;

	MPI_Type_get_envelope(type, &ptype->nints, &ptype->naddrs,
	                      &ptype->ndatatypes, &ptype->combiner);

	if (ptype->nints) {
		ptype->integers = malloc(sizeof(int)*ptype->nints);
		if (!ptype->integers) {
			fprintf(stderr, "photon_decode_MPI_Datatype(): out of memory");
			return -1;
		}
	}

	if (ptype->naddrs) {
		ptype->addresses = malloc(sizeof(MPI_Aint)*ptype->naddrs);
		if (!ptype->addresses) {
			fprintf(stderr, "photon_decode_MPI_Datatype(): out of memory");
			goto error_exit_addresses;
		}
	}

	if (ptype->ndatatypes) {
		types = malloc(sizeof(MPI_Datatype)*ptype->ndatatypes);
		ptype->datatypes = malloc(sizeof(int)*ptype->ndatatypes);
		if (!types || !ptype->datatypes) {
			fprintf(stderr, "photon_decode_MPI_Datatype(): out of memory");
			goto error_exit_datatypes;
		}
	}

	MPI_Type_get_contents(type, ptype->nints, ptype->naddrs, ptype->ndatatypes,
	                      ptype->integers, ptype->addresses, types);

	/* Transform MPI_Datatypes to our own mapping to send over the wire.
	 * There might be a better way to do this.
	 */
	for (i = 0; i < ptype->ndatatypes; i++) {
		if (types[i] == MPI_DOUBLE)
			ptype->datatypes[i] = PHOTON_MPI_DOUBLE;
		else
			ptype->datatypes[i] = -1;
	}

	return 0;

error_exit_datatypes:
	free(ptype->addresses);
error_exit_addresses:
	free(ptype->integers);
	return -1;
}

#define INT_ASSIGN_MOVE(ptr, i) do { \
				*((int *)ptr) = i; \
				ptr += sizeof(int); \
} while(0)

inline void photon_destroy_mpi_datatype (PhotonMPIDatatype *pd) {
	if (pd->nints)			free(pd->integers);
	if (pd->naddrs)			free(pd->addresses);
	if (pd->ndatatypes) free(pd->datatypes);
}

void print_photon_io_info(PhotonIOInfo *io) {
	fprintf(stderr, "PhotonIOInfo:\n"
	        "fileURI		= %s\n"
	        "amode			= %d\n"
	        "niter			= %d\n"
	        "v.combiner = %d\n"
	        "v.nints		= %d\n"
	        "v.ints[0]	= %d\n"
	        "v.naddrs		= %d\n"
	        "v.ndts			= %d\n"
	        "v.dts[0]		= %d\n",
	        io->fileURI, io->amode, io->niter, io->view.combiner,
	        io->view.nints, io->view.integers[0], io->view.naddrs,
	        io->view.ndatatypes, io->view.datatypes[0]
	       );
}

/* See photon_xsp.h for message format */
void *photon_create_xsp_io_init_msg(PhotonIOInfo *io, int *size) {
	void *msg;
	void *msg_ptr;
	int totalsize = 0;

	totalsize += sizeof(int) + strlen(io->fileURI) + 1;
	totalsize += sizeof(int)*3;
	totalsize += sizeof(int) + io->view.nints*sizeof(int);
	totalsize += sizeof(int) + io->view.naddrs*sizeof(MPI_Aint);
	totalsize += sizeof(int) + io->view.ndatatypes*sizeof(int);

	msg_ptr = msg = malloc(totalsize);
	if (!msg) {
		log_err("photon_create_xsp_io_init_msg(): out of memory");
		return NULL;
	}

	INT_ASSIGN_MOVE(msg_ptr, strlen(io->fileURI) + 1);
	strcpy((char*)msg_ptr, io->fileURI);
	msg_ptr += strlen(io->fileURI) + 1;

	INT_ASSIGN_MOVE(msg_ptr, io->amode);
	INT_ASSIGN_MOVE(msg_ptr, io->niter);
	INT_ASSIGN_MOVE(msg_ptr, io->view.combiner);

	INT_ASSIGN_MOVE(msg_ptr, io->view.nints);
	memcpy(msg_ptr, io->view.integers, io->view.nints*sizeof(int));
	msg_ptr += io->view.nints*sizeof(int);

	INT_ASSIGN_MOVE(msg_ptr, io->view.naddrs);
	memcpy(msg_ptr, io->view.addresses, io->view.naddrs*sizeof(MPI_Aint));
	msg_ptr += io->view.naddrs*sizeof(MPI_Aint);

	INT_ASSIGN_MOVE(msg_ptr, io->view.ndatatypes);
	memcpy(msg_ptr, io->view.datatypes, io->view.ndatatypes*sizeof(int));

	*size = totalsize;
	return msg;
}

// stub function for higher-level I/O operation
int verbs_xsp_phorwarder_io_init(char *file, int amode, MPI_Datatype view, int niter) {
	PhotonIOInfo io;
	void *msg;
	int msg_size;

	ctr_info(" > verbs_xsp_phorwarder_io_init()");

	if( __initialized <= 0 ) {
		log_err("verbs_xsp_phorwarder_io_init(): Library not initialized.	 Call photon_xsp_init() first");
		return -1;
	}

	io.fileURI = file;
	io.amode = amode;
	io.niter = niter;

	if (photon_decode_MPI_Datatype(view, &io.view) != 0)
		return -1;

	msg = photon_create_xsp_io_init_msg(&io, &msg_size);
	if (msg == NULL) {
		photon_destroy_mpi_datatype(&io.view);
		return -1;
	}

	print_photon_io_info(&io);

	if (xsp_send_msg(verbs_processes[_photon_fp].sess, msg, msg_size, PHOTON_IO) <= 0) {
		log_err("verbs_xsp_phorwarder_io_init(): Couldn't send IO info");
		photon_destroy_mpi_datatype(&io.view);
		free(msg);
		return -1;
	}

	/* TODO: Maybe we should receive an ACK? */

	photon_destroy_mpi_datatype(&io.view);
	free(msg);

	ctr_info(" > verbs_xsp_phorwarder_io_init() completed.");
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// the actual photon XSP API (phorwarder)

int photon_xsp_init(int nproc, int myrank, MPI_Comm comm, int *phorwarder) {
	int ret;
	if((ret = verbs_init(nproc, myrank, comm, 1)) != 0)
		return ret;
	*phorwarder = _photon_fp;
	ctr_info(" > photon_xsp_init(%d, %d): %d", nproc, myrank, _photon_fp);
	return 0;
}

inline int photon_xsp_init_server(int nproc) {
	return verbs_xsp_init_server(nproc);
}

inline int photon_xsp_phorwarder_io_init(char *file, int amode, MPI_Datatype view, int niter) {
	return verbs_xsp_phorwarder_io_init(file, amode, view, niter);
}

int photon_xsp_phorwarder_io_finalize() {
	return 0;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
// Util methods for the XSP libphoton server implementation

int verbs_xsp_register_session(libxspSess *sess) {
	int i;

	pthread_mutex_lock(&sess_mtx);
	if (sess_count >= _photon_nproc) {
		log_err("verbs_xsp_register_session(): Error: out of active DAT buffers!");
		pthread_mutex_unlock(&sess_mtx);
		return -1;
	}

	// find a process struct that has no session...
	// proc _photon_nproc has the phorwarder server info

	for (i = 0; i < _photon_nproc-1; i++) {
		if (!verbs_processes[i].sess)
			break;
	}

	dbg_info("verbs_xsp_register_session(): registering session to proc: %d", i);

	verbs_processes[i].sess = sess;

	sess_count++;
	pthread_mutex_unlock(&sess_mtx);

	return 0;
}

int verbs_xsp_unregister_session(libxspSess *sess) {
	int ind;

	pthread_mutex_lock(&sess_mtx);
	if (verbs_xsp_lookup_proc(sess, &ind) < 0) {
		log_err("verbs_xsp_dergister_session(): Couldn't find proc associated with session");
		pthread_mutex_unlock(&sess_mtx);
		return -1;
	}

	verbs_processes[ind].sess = NULL;

	sess_count--;
	pthread_mutex_unlock(&sess_mtx);

	return 0;
}

int verbs_xsp_get_local_ci(libxspSess *sess, verbs_cnct_info_t **ci) {
	int i;
	int ind;

	ctr_info(" > verbs_xsp_get_local_ci()");

	if( __initialized <= 0 ) {
		log_err("verbs_xsp_get_local_ci(): Library not initialized.	 Call photon_xsp_init_server() first");
		goto error_exit;
	}

	if (verbs_xsp_lookup_proc(sess, &ind) < 0) {
		log_err("verbs_xsp_set_ri(): Couldn't find proc associated with session");
		return -1;
	}

	*ci = (struct verbs_cnct_info *)malloc(MAX_QP*sizeof(struct verbs_cnct_info));
	if( !*ci ) {
		goto error_exit;
	}

	for(i=0; i<MAX_QP; ++i) {
		(*ci)[i].lid = phot_verbs_lid;
		(*ci)[i].qpn = verbs_processes[ind].qp[i]->qp_num;
		(*ci)[i].psn = (lrand48() & 0xfff000) + _photon_fp+1;
	}

	return 0;

error_exit:
	return -1;
}

int verbs_xsp_server_connect_peer(libxspSess *sess, verbs_cnct_info_t *local_ci, verbs_cnct_info_t *remote_ci) {
	int ind;

	ctr_info(" > verbs_xsp_server_connect_peer()");

	if( __initialized <= 0 ) {
		log_err("verbs_xsp_server_connect_peer(): Library not initialized.	Call photon_xsp_init_server() first");
		goto error_exit;
	}

	if (verbs_xsp_lookup_proc(sess, &ind) < 0) {
		log_err("verbs_xsp_server_connect_peer(): Couldn't find proc associated with session");
		goto error_exit;
	}

	if (verbs_connect_qps(MAX_QP, local_ci, remote_ci, &verbs_processes[ind]) != 0) {
		log_err("verbs_xsp_server_connect_peer(): Cannot connect queue pairs");
		goto error_exit;
	}

	return 0;

error_exit:
	return -1;
}

int verbs_xsp_set_ri(libxspSess *sess, PhotonLedgerInfo *ri, PhotonLedgerInfo **ret_ri) {
	int ind;

	ctr_info(" > verbs_xsp_set_ri()");

	if (verbs_xsp_lookup_proc(sess, &ind) < 0) {
		log_err("verbs_xsp_set_ri(): Couldn't find proc associated with session");
		return -1;
	}

	if (!ri)
		return -1;

	verbs_processes[ind].remote_rcv_info_ledger->remote.rkey = ri->rkey;
	verbs_processes[ind].remote_rcv_info_ledger->remote.addr = ri->va;

	dbg_info("Setting rcv_info ledger info of %d: %"PRIxPTR, ind, ri->va);

	*ret_ri = (PhotonLedgerInfo*)malloc(sizeof(PhotonLedgerInfo));
	if( !*ret_ri ) {
		return -1;
	}

	(*ret_ri)->rkey = shared_storage->mr->rkey;
	(*ret_ri)->va = (uintptr_t)(verbs_processes[ind].local_rcv_info_ledger->entries);

	return 0;
}

int verbs_xsp_set_si(libxspSess *sess, PhotonLedgerInfo *si, PhotonLedgerInfo **ret_si) {
	int ind;

	ctr_info(" > verbs_xsp_set_si()");

	if (verbs_xsp_lookup_proc(sess, &ind) < 0) {
		log_err("verbs_xsp_set_si(): Couldn't find proc associated with session");
		return -1;
	}

	if (!si)
		return -1;

	verbs_processes[ind].remote_snd_info_ledger->remote.rkey = si->rkey;
	verbs_processes[ind].remote_snd_info_ledger->remote.addr = si->va;

	dbg_info("Setting snd_info ledger info of %d: %"PRIxPTR, ind, si->va);

	*ret_si = (PhotonLedgerInfo*)malloc(sizeof(PhotonLedgerInfo));
	if( !*ret_si ) {
		return -1;
	}

	(*ret_si)->rkey = shared_storage->mr->rkey;
	(*ret_si)->va = (uintptr_t)(verbs_processes[ind].local_snd_info_ledger->entries);

	return 0;
}

int verbs_xsp_set_fi(libxspSess *sess, PhotonLedgerInfo *fi, PhotonLedgerInfo **ret_fi) {
	int ind;

	ctr_info(" > verbs_xsp_set_fi()");

	if (verbs_xsp_lookup_proc(sess, &ind) < 0) {
		log_err("verbs_xsp_set_ri(): Couldn't find proc associated with session");
		return -1;
	}

	if (!fi)
		return -1;

	verbs_processes[ind].remote_FIN_ledger->remote.rkey = fi->rkey;
	verbs_processes[ind].remote_FIN_ledger->remote.addr = fi->va;

	*ret_fi = (PhotonLedgerInfo*)malloc(sizeof(PhotonLedgerInfo));
	if( !*ret_fi ) {
		return -1;
	}

	(*ret_fi)->rkey = shared_storage->mr->rkey;
	(*ret_fi)->va = (uintptr_t)(verbs_processes[ind].local_FIN_ledger->entries);

	return 0;
}

int verbs_xsp_set_io(libxspSess *sess, PhotonIOInfo *io) {
	int ind;

	if (verbs_xsp_lookup_proc(sess, &ind) < 0) {
		log_err("verbs_xsp_set_io(): Couldn't find proc associated with session");
		return -1;
	}

	verbs_processes[ind].io_info = io;

	return 0;
}

int verbs_xsp_do_io(libxspSess *sess) {
	int i;
	int ind;
	int ndimensions;
	int bufsize;
	char *filename;
	FILE *file;
	void *buf[2];
	MPI_Aint dtextent;
	uint32_t request;
	ProcessInfo *p;

	if (verbs_xsp_lookup_proc(sess, &ind) < 0) {
		log_err("verbs_xsp_do_io(): Couldn't find proc associated with session");
		return -1;
	}

	p = &verbs_processes[ind];

	if (!p->io_info) {
		log_err("verbs_xsp_do_io(): Trying to do I/O without I/O Info set");
		return -1;
	}

	/* TODO: So this is how I think this will go: */
	if (p->io_info->view.combiner != MPI_COMBINER_SUBARRAY) {
		log_err("verbs_xsp_do_io(): Unsupported combiner");
		return -1;
	}

	/* We can't do this because it requires MPI_Init. We need to figure out
	 * the best way to support MPI_Datatypes. Also, OpenMPI doesn't use simple
	 * int constants like MPICH2, so I'm not sure how we would transfer these.
	 *
	 * MPI_Type_get_true_extent(p->io_info->view.datatypes[0], &dtlb, &dtextent);
	 */
	if (p->io_info->view.datatypes[0] == PHOTON_MPI_DOUBLE) {
		dtextent = 8;
	}
	else {
		log_err("verbs_xsp_do_io(): Unsupported datatype");
		return -1;
	}

	bufsize = dtextent;
	ndimensions = p->io_info->view.integers[0];
	for(i = ndimensions+1; i <= 2*ndimensions; i++) {
		bufsize *= p->io_info->view.integers[i];
	}

	buf[0] = malloc(bufsize);
	buf[1] = malloc(bufsize);
	if (buf[0] == NULL || buf[1] == NULL) {
		log_err("verbs_xsp_do_io(): Out of memory");
		return -1;
	}

	if(verbs_register_buffer(buf[0], bufsize) != 0) {
		log_err("verbs_xsp_do_io(): Couldn't register first receive buffer");
		return -1;
	}

	if(verbs_register_buffer(buf[1], bufsize) != 0) {
		log_err("verbs_xsp_do_io(): Couldn't register second receive buffer");
		return -1;
	}

	/* For now we just write locally (fileURI is a local path on phorwarder) */
	filename = malloc(strlen(p->io_info->fileURI) + 10);
	sprintf(filename, "%s_%d", p->io_info->fileURI, ind);
	file = fopen(filename, "w");
	if (file == NULL) {
		log_err("verbs_xsp_do_io(): Couldn't open local file %s", filename);
		return -1;
	}

	verbs_post_recv_buffer_rdma(ind, buf[0], bufsize, 0, &request);
	/* XXX: is the index = rank? FIXME: not right now! */
	for (i = 1; i < p->io_info->niter; i++) {
		verbs_wait(request);
		/* Post the second buffer so we can overlap the I/O */
		verbs_post_recv_buffer_rdma(ind, buf[i%2], bufsize, i, &request);

		if(fwrite(buf[(i-1)%2], 1, bufsize, file) != bufsize) {
			log_err("verbs_xsp_do_io(): Couldn't write to local file %s: %m", filename);
			return -1;
		}
		/* For now we just write locally, AFAIK this is blocking */
		/*
		 * Process the buffer:
		 * This is one iteration of (I/O) data in a contiguous buffer.
		 * The actual data layout is specified by p->io_info.view.
		 * Basically what we do is have p->io_info.view described on the
		 * eXnode as the metadata for what this particular node is writing.
		 * Each node will write to its own file and we keep track of the
		 * offsets using p->io_info.view and the iteration number.
		 * Right now I'm assuming only new files and write only.
		 *
		 * TODO: add 'file offset' to PhotonIOInfo so we can keep track.
		 *	 This should also allow the client to start writing at any place
		 *	 in the file.
		 *
		 * So this buffer would actually need to be moved somewhere else
		 * where we manage the transfer to the I/O server (or write it locally).
		 */
	}
	/* wait for last write */
	verbs_wait(request);
	if(fwrite(buf[(i-1)%2], 1, bufsize, file) != bufsize) {
		log_err("verbs_xsp_do_io(): Couldn't write to local file %s: %m", filename);
		return -1;
	}
	fclose(file);
	free(filename);
	free(buf[0]);
	free(buf[1]);

	return 0;
}

#endif

