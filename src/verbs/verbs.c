#include "mpi.h"

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#include "verbs.h"
#include "verbs_connect.h"
#include "verbs_exchange.h"
#include "verbs_buffer.h"
#include "verbs_remote_buffer.h"
#include "htable.h"
#include "buffertable.h"
#include "logging.h"
#include "counter.h"

extern int _photon_nproc;
extern int _photon_myrank;
extern int _photon_forwarder;
static MPI_Comm _photon_comm;

int verbs_initialized(void);
int verbs_init(photonConfig cfg);
int verbs_finalize(void);
int verbs_register_buffer(char *buffer, int buffer_size);
int verbs_unregister_buffer(char *buffer, int size);
int verbs_test(uint32_t request, int *flag, int *type, void *status);
int verbs_wait(uint32_t request);
int verbs_wait_ledger(uint32_t request);
int verbs_post_recv_buffer_rdma(int proc, char *ptr, uint32_t size, int tag, uint32_t *request);
int verbs_post_send_buffer_rdma(int proc, char *ptr, uint32_t size, int tag, uint32_t *request);
int verbs_post_send_request_rdma(int proc, uint32_t size, int tag, uint32_t *request);
int verbs_wait_recv_buffer_rdma(int proc, int tag);
int verbs_wait_send_buffer_rdma(int proc, int tag);
int verbs_wait_send_request_rdma(int tag);
int verbs_post_os_put(int proc, char *ptr, uint32_t size, int tag, uint32_t remote_offset, uint32_t *request);
int verbs_post_os_put(int proc, char *ptr, uint32_t size, int tag, uint32_t remote_offset, uint32_t *request);
int verbs_post_os_get(int proc, char *ptr, uint32_t size, int tag, uint32_t remote_offset, uint32_t *request);
int verbs_send_FIN(int proc);
int verbs_wait_any(int *ret_proc, uint32_t *ret_req);
int verbs_wait_any_ledger(int *ret_proc, uint32_t *ret_req);

/* 
   We only want to spawn a dedicated thread for ledgers on
   multithreaded instantiations of the library (e.g. in xspd).
   FIXME: All of the pthreads stuff below should also depend on this.
*/
#ifdef PHOTON_MULTITHREADED
static pthread_t ledger_watcher;
static void *__verbs_req_watcher(void *arg);
#else
static int __verbs_wait_event(verbs_req_t *req);
static int __verbs_wait_ledger(verbs_req_t *req);
static int __verbs_nbpop_ledger(verbs_req_t *req);
static int __verbs_nbpop_event(verbs_req_t *req);
#endif

verbs_buffer_t *shared_storage;

static ProcessInfo *verbs_processes;
static verbs_cnct_ctx verbs_ctx = {
	.ib_dev ="ib0",
	.ib_port = 1,
	.ib_context = NULL,
	.ib_pd = NULL,
	.ib_cq = NULL,
    .ib_srq = NULL,
    .ib_cc = NULL,
	.ib_lid = 0,
	.verbs_processes = NULL,
	.cm_schannel = NULL,
	.cm_rchannel = NULL,
    .cm_id = NULL,
    .tx_depth = 16,
    .rx_depth = 16
};

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

static inline verbs_req_t *__verbs_get_request();

#ifdef DEBUG
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

/* we are now a Photon backend */
struct photon_backend_t photon_verbs_backend = {
	.initialized = verbs_initialized,
    .init = verbs_init,
    .finalize = verbs_finalize,
    .register_buffer = verbs_register_buffer,
    .unregister_buffer = verbs_unregister_buffer,
    .test = verbs_test,
    .wait = verbs_wait,
    .wait_ledger = verbs_wait,
    .post_recv_buffer_rdma = verbs_post_recv_buffer_rdma,
    .post_send_buffer_rdma = verbs_post_send_buffer_rdma,
    .wait_recv_buffer_rdma = verbs_wait_recv_buffer_rdma,
    .wait_send_buffer_rdma = verbs_wait_send_buffer_rdma,
    .wait_send_request_rdma = verbs_wait_send_request_rdma,
    .post_os_put = verbs_post_os_put,
    .post_os_get = verbs_post_os_get,
    .send_FIN = verbs_send_FIN,
#ifndef PHOTON_MULTITHREADED
    .wait_any = verbs_wait_any,
    .wait_any_ledger = verbs_wait_any_ledger
#endif
};


static inline verbs_req_t *__verbs_get_request() {
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

int verbs_initialized() {
	if (__initialized)
		return PHOTON_OK;
	else
		return PHOTON_ERROR_NOINIT;
}

int __verbs_init_common(photonConfig cfg) {
	int i;
	char *buf;
	int bufsize, offset;
	int info_ledger_size, FIN_ledger_size;

	if (__initialized != 0) {
		log_err("Error: already initialized/initializing");
		goto error_exit;
	}

	srand48(getpid() * time(NULL));

	_photon_nproc = cfg->nproc;
	_photon_myrank = (int)cfg->address;
	_photon_forwarder = cfg->use_forwarder;
	_photon_comm = cfg->comm;

	dbg_info("(nproc %d, rank %d)",_photon_nproc, _photon_myrank);

	if (cfg->ib_dev)
		verbs_ctx.ib_dev = cfg->ib_dev;

	if (cfg->ib_port)
	    verbs_ctx.ib_port = cfg->ib_port;

	if (cfg->use_cma && !cfg->eth_dev) {
		log_err("CMA specified but Ethernet dev missing");
		goto error_exit;
	}

	// __initialized: 0 - not; -1 - initializing; 1 - initialized
	__initialized = -1;
	INIT_COUNTER(curr_cookie, 1);
	INIT_COUNTER(handshake_rdma_write, 0);

	requests = malloc(sizeof(verbs_req_t) * DEF_NUM_REQUESTS);
	if (!requests) {
		log_err("Failed to allocate request list");
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
		log_err("Failed to allocate request table");
		goto error_exit_bt;
	}

	dbg_info("create_ledger_reqtable()");

	ledger_reqtable = htable_create(193);
	if (!ledger_reqtable) {
		log_err("Failed to allocate request table");
		goto error_exit_rt;
	}

	verbs_processes = (ProcessInfo *) malloc(sizeof(ProcessInfo) * (_photon_nproc));
	if (!verbs_processes) {
		log_err("Couldn't allocate process information");
		goto error_exit_lrt;
	}

	// Set it to zero, so that we know if it ever got initialized
	memset(verbs_processes, 0, sizeof(ProcessInfo) * (_photon_nproc));

	for(i = 0; i < _photon_nproc; i++) {
		verbs_processes[i].curr_remote_buffer = __verbs_remote_buffer_create();
		if(!verbs_processes[i].curr_remote_buffer) {
			log_err("Couldn't allocate process remote buffer information");
			goto error_exit_gp;
		}
	}

	// keep a pointer to the processes list in the verbs context
	verbs_ctx.verbs_processes = verbs_processes;

	dbg_info("alloc'd process info");

	if(__verbs_init_context(&verbs_ctx)) {
		log_err("Could not initialize verbs context");
		goto error_exit_crb;
	}

	if(__verbs_connect_peers(&verbs_ctx)) {
		log_err("Could not connect peers");
		goto error_exit_crb;
	}

	// Everything is x2 cause we need a local and a remote copy of each ledger.
	// Remote Info (_ri_) ledger has an additional x2 cause we need "send-info" and "receive-info" ledgers.
	info_ledger_size = 2 * 2 * sizeof(verbs_ri_ledger_entry_t) * LEDGER_SIZE * (_photon_nproc);
	FIN_ledger_size  = 2 * sizeof(verbs_rdma_FIN_ledger_entry_t) * LEDGER_SIZE * (_photon_nproc);
	bufsize = info_ledger_size + FIN_ledger_size;
	buf = malloc(bufsize);
	if (!buf) {
		log_err("Couldn't allocate ledgers");
		goto error_exit_verbs_cnct;
	}
	dbg_info("Bufsize: %d", bufsize);

	shared_storage = __verbs_buffer_create(buf, bufsize);
	if (!shared_storage) {
		log_err("Couldn't register shared storage");
		goto error_exit_buf;
	}

	if (__verbs_buffer_register(shared_storage, verbs_ctx.ib_pd) != 0) {
		log_err("couldn't register local buffer for the ledger entries");
		goto error_exit_ss;
	}

	if (__verbs_setup_ri_ledgers(verbs_processes, buf, LEDGER_SIZE) != 0) {
		log_err("verbs_init_common(); couldn't setup snd/rcv info ledgers");
		goto error_exit_listeners;
	}

	// skip 4 ledgers (rcv info local, rcv info remote, snd info local, snd info remote)
	offset = 4 * sizeof(verbs_ri_ledger_entry_t) * LEDGER_SIZE * (_photon_nproc);
	if (__verbs_setup_FIN_ledger(verbs_processes, buf + offset, LEDGER_SIZE) != 0) {
		log_err("verbs_init_common(); couldn't setup send ledgers");
		goto error_exit_ri_ledger;
	}

#ifdef PHOTON_MULTITHREADED
	if (pthread_create(&ledger_watcher, NULL, __verbs_req_watcher, NULL)) {
		log_err("pthread_create() failed");
		goto error_exit_ledger_watcher;
	}
#endif

	dbg_info("ended successfully =============");

	return 0;

#ifdef PHOTON_MULTITHREADED
error_exit_ledger_watcher:
#endif
error_exit_ri_ledger:
error_exit_listeners:
error_exit_ss:
	__verbs_buffer_free(shared_storage);
error_exit_buf:
	if (buf)
		free(buf);
error_exit_verbs_cnct:
error_exit_crb:
	for(i = 0; i < _photon_nproc; i++) {
		if (verbs_processes[i].curr_remote_buffer != NULL) {
			__verbs_remote_buffer_free(verbs_processes[i].curr_remote_buffer);
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

	if (__verbs_init_common(cfg) != 0)
		goto error_exit;

	if (__verbs_exchange_ri_ledgers(verbs_processes) != 0) {
		log_err("verbs_init(); couldn't exchange rdma ledgers");
		goto error_exit_listeners;
	}

	if (__verbs_exchange_FIN_ledger(verbs_processes) != 0) {
		log_err("verbs_init(); couldn't exchange send ledgers");
		goto error_exit_FIN_ledger;
	}

	while( !SLIST_EMPTY(&pending_mem_register_list) ) {
		struct mem_register_req *mem_reg_req;
	    dbg_info("registering buffer in queue");
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
	dbg_info("ended successfully =============");

	return 0;

error_exit_FIN_ledger:
error_exit_listeners:
	if (shared_storage->buffer)
		free(shared_storage->buffer);
	__verbs_buffer_free(shared_storage);
	for(i = 0; i < _photon_nproc; i++) {
		if (verbs_processes[i].curr_remote_buffer != NULL) {
			__verbs_remote_buffer_free(verbs_processes[i].curr_remote_buffer);
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

int verbs_finalize() {
	return 0;
}

int verbs_register_buffer(char *buffer, int buffer_size) {
	static int first_time = 1;
	verbs_buffer_t *db;

	dbg_info("(%p, %d)",buffer, buffer_size);

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
		dbg_info("called before init, queueing buffer info");
		goto normal_exit;
	}

	if (buffertable_find_exact((void *)buffer, buffer_size, &db) == 0) {
		dbg_info("we had an existing buffer, reusing it");
		db->ref_count++;
		goto normal_exit;
	}

	db = __verbs_buffer_create(buffer, buffer_size);
	if (!db) {
		log_err("Couldn't register shared storage");
		goto error_exit;
	}

	dbg_info("created buffer: %p", db);

	if (__verbs_buffer_register(db, verbs_ctx.ib_pd) != 0) {
		log_err("Couldn't register buffer");
		goto error_exit_db;
	}

	dbg_info("registered buffer");

	if (buffertable_insert(db) != 0) {
		goto error_exit_db;
	}

	dbg_info("added buffer to hash table");

normal_exit:
	return 0;
error_exit_db:
	__verbs_buffer_free(db);
error_exit:
	return -1;
}

int verbs_unregister_buffer(char *buffer, int size) {
	verbs_buffer_t *db;

	dbg_info();

	if( __initialized == 0 ) {
		log_err("Library not initialized.	Call photon_init() first");
		goto error_exit;
	}

	if (buffertable_find_exact((void *)buffer, size, &db) != 0) {
		dbg_info("no such buffer is registered");
		return 0;
	}

	if (--(db->ref_count) == 0) {
		if (__verbs_buffer_unregister(db) != 0) {
			goto error_exit;
		}
		buffertable_remove( db );
		__verbs_buffer_free(db);
	}

	return 0;

error_exit:
	return -1;
}

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
int verbs_test(uint32_t request, int *flag, int *type, void *status) {
	verbs_req_t *req;
	void *test;
	int ret_val;

	dbg_info("(%d)",request);

	if( __initialized <= 0 ) {
		log_err("Library not initialized.	 Call photon_init() first");
		dbg_info("returning -1");
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
			dbg_info("returning 1, flag:-1");
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
		//if( status != MPI_STATUS_IGNORE ) {
		//	status->MPI_SOURCE = req->proc;
		//	status->MPI_TAG = req->tag;
		//	status->MPI_ERROR = 0; // FIXME: Make sure that "0" means success in MPI?
		//}
		dbg_info("returning 0, flag:1");
		return 0;
	}
	else if( ret_val > 0 ) {
		dbg_info("returning 0, flag:0");
		*flag = 0;
		return 0;
	}
	else {
		dbg_info("returning -1, flag:0");
		*flag = 0;
		return -1;
	}
}

///////////////////////////////////////////////////////////////////////////////
int verbs_wait(uint32_t request) {
	verbs_req_t *req;

	dbg_info("(%d)",request);

	if( __initialized <= 0 ) {
		log_err("Library not initialized.	 Call photon_init() first");
		return -1;
	}

	if (htable_lookup(reqtable, (uint64_t)request, (void**)&req) != 0) {
		if (htable_lookup(ledger_reqtable, (uint64_t)request, (void**)&req) != 0) {
			log_err("Wrong request value, operation not in table");
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
				dbg_info("removing ledger RDMA: %u", req->id);
				htable_remove(ledger_reqtable, (uint64_t)req->id, NULL);
				SAFE_LIST_REMOVE(req, list);
				SAFE_LIST_INSERT_HEAD(&free_reqs_list, req, list);
				dbg_info("%d requests left in ledgertable", htable_count(ledger_reqtable));
			}
		}
		else {
			if (htable_lookup(reqtable, (uint64_t)req->id, NULL) != -1) {
				dbg_info("removing event with cookie:%u", req->id);
				htable_remove(reqtable, (uint64_t)req->id, NULL);
				SAFE_LIST_REMOVE(req, list);
				SAFE_LIST_INSERT_HEAD(&free_reqs_list, req, list);
				dbg_info("%d requests left in reqtable", htable_count(reqtable));
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

	dbg_info("remaining: %d+%d", htable_count(reqtable), GET_COUNTER(handshake_rdma_write));

	if( __initialized <= 0 ) {
		log_err("Library not initialized.	 Call photon_init() first");
		goto error_exit;
	}

	if ( (htable_count(reqtable) == 0) && (GET_COUNTER(handshake_rdma_write) == 0) ) {
		dbg_info("No events on queue, or handshake writes pending to wait on");
		goto error_exit;
	}

	do {
		ne = ibv_poll_cq(verbs_ctx.ib_cq, 1, &wc);
		if (ne < 0) {
			log_err("ibv_poll_cq() failed");
			goto error_exit;
		}
	}
	while (ne < 1);

	if (wc.status != IBV_WC_SUCCESS) {
		log_err("(status==%d) != IBV_WC_SUCCESS\n",wc.status);
		goto error_exit;
	}

	cookie = (uint32_t)( (wc.wr_id<<32)>>32);
	dbg_info("received event with cookie:%u", cookie);

	if (htable_lookup(reqtable, (uint64_t)cookie, &test) == 0) {
		tmp_req = test;

		tmp_req->state = REQUEST_COMPLETED;
		SAFE_LIST_REMOVE(tmp_req, list);
		SAFE_LIST_INSERT_HEAD(&unreaped_evd_reqs_list, tmp_req, list);
	}
	else if (htable_lookup(ledger_reqtable, (uint64_t)cookie, &test) == 0) {
		if( DEC_COUNTER(handshake_rdma_write) <= 0 ) {
			log_err("handshake_rdma_write_count is negative");
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
		log_err("Library not initialized.	 Call photon_init() first");
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
			ne = ibv_poll_cq(verbs_ctx.ib_cq, 1, &wc);
			if (ne < 0) {
				fprintf(stderr, "poll CQ failed %d\n", ne);
				return 1;
			}
		}
		while (ne < 1);

		if (wc.status != IBV_WC_SUCCESS) {
			log_err("(status==%d) != IBV_WC_SUCCESS\n",wc.status);
			goto error_exit;
		}

		cookie = (uint32_t)( (wc.wr_id<<32)>>32);
		//fprintf(stderr,"[%d/%d] __verbs_wait_event(): Event occured with cookie:%x\n", _photon_myrank, _photon_nproc, cookie);
		if (cookie == req->id) {
			req->state = REQUEST_COMPLETED;

			dbg_info("removing event with cookie:%u", cookie);
			htable_remove(reqtable, (uint64_t)req->id, NULL);
			SAFE_LIST_REMOVE(req, list);
			SAFE_LIST_INSERT_HEAD(&free_reqs_list, req, list);
			dbg_info("%d requests left in reqtable", htable_count(reqtable));
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
					log_err("handshake_rdma_write_count is negative");
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

	dbg_info("(%d)",req->id);

	if( __initialized <= 0 ) {
		log_err("Library not initialized.	Call photon_init() first");
		return -1;
	}

	if(req->state == REQUEST_PENDING) {
		uint32_t cookie;

		ne = ibv_poll_cq(verbs_ctx.ib_cq, 1, &wc);
		if (ne < 0) {
			log_err("ibv_poll_cq() failed");
			goto error_exit;
		}
		if (!ne) {
			return 1;
		}

		if (wc.status != IBV_WC_SUCCESS) {
			log_err("(status==%d) != IBV_WC_SUCCESS\n",wc.status);
			goto error_exit;
		}

		cookie = (uint32_t)( (wc.wr_id<<32)>>32);
//fprintf(stderr,"__gen_nbpop_event() poped an events with cookie:%x\n",cookie);
		if (cookie == req->id) {
			req->state = REQUEST_COMPLETED;

			dbg_info("removing event with cookie:%u", cookie);
			htable_remove(reqtable, (uint64_t)req->id, NULL);
			SAFE_LIST_REMOVE(req, list);
			SAFE_LIST_INSERT_HEAD(&free_reqs_list, req, list);
			dbg_info("%d requests left in reqtable", htable_count(reqtable));
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
					log_err("handshake_rdma_write_count is negative");
				}
			}
		}
	}

	dbg_info("returning %d", (req->state == REQUEST_COMPLETED)?0:1 );
	return (req->state == REQUEST_COMPLETED)?0:1;

error_exit:
	return -1;
}

///////////////////////////////////////////////////////////////////////////////
static int __verbs_wait_ledger(verbs_req_t *req) {
	void *test;
	int curr, num_entries, i=-1;

	dbg_info("(%d)",req->id);

	if( __initialized <= 0 ) {
		log_err("Library not initialized.	Call photon_init() first");
		return -1;
	}

#ifdef DEBUG
	for(i = 0; i < _photon_nproc; i++) {
		verbs_rdma_FIN_ledger_entry_t *curr_entry;
		curr = verbs_processes[i].local_FIN_ledger->curr;
		curr_entry = &(verbs_processes[i].local_FIN_ledger->entries[curr]);
		dbg_info("curr_entry(proc==%d)=%p",i,curr_entry);
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
	dbg_info("removing RDMA: %u/%u", i, req->id);
	htable_remove(ledger_reqtable, (uint64_t)req->id, NULL);
	SAFE_LIST_REMOVE(req, list);
	SAFE_LIST_INSERT_HEAD(&free_reqs_list, req, list);
	dbg_info("%d requests left in reqtable", htable_count(ledger_reqtable));

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

	dbg_info("(%d)",req->id);

	if( __initialized <= 0 ) {
		log_err("Library not initialized.	 Call photon_init() first");
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
				dbg_info("Found curr:%d req:%u while looking for req:%u", curr, curr_entry->request, req->id);
				curr_entry->header = 0;
				curr_entry->footer = 0;

				if (curr_entry->request == req->id) {
					req->state = REQUEST_COMPLETED;
					dbg_info("removing RDMA i:%u req:%u", i, req->id);
					htable_remove(ledger_reqtable, (uint64_t)req->id, NULL);
					LIST_REMOVE(req, list);
					LIST_INSERT_HEAD(&free_reqs_list, req, list);
					int num = verbs_processes[i].local_FIN_ledger->num_entries;
					int new_curr = (verbs_processes[i].local_FIN_ledger->curr + 1) % num;
					verbs_processes[i].local_FIN_ledger->curr = new_curr;
					dbg_info("returning 0");
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
		dbg_info("req->state != PENDING, returning 0");
		return 0;
	}

	dbg_info("at end, returning %d",(req->state == REQUEST_COMPLETED)?0:1);
	return (req->state == REQUEST_COMPLETED)?0:1;
}

///////////////////////////////////////////////////////////////////////////////
int verbs_wait_any(int *ret_proc, uint32_t *ret_req) {
	struct ibv_wc wc;

	dbg_info("remaining: %d", htable_count(reqtable));

	if( __initialized <= 0 ) {
		log_err("Library not initialized.	 Call photon_init() first");
		goto error_exit;
	}

	if (ret_req == NULL) {
		goto error_exit;
	}

	if (htable_count(reqtable) == 0) {
		log_err("No events on queue to wait on");
		goto error_exit;
	}

	while(1) {
		uint32_t cookie;
		int existed, ne;

		ne = ibv_poll_cq(verbs_ctx.ib_cq, 1, &wc);
		if (ne < 0) {
			log_err("ibv_poll_cq() failed");
			goto error_exit;
		}
		if (!ne) {
			goto error_exit;
		}

		if (wc.status != IBV_WC_SUCCESS) {
			log_err("(status==%d) != IBV_WC_SUCCESS\n",wc.status);
			goto error_exit;
		}

		cookie = (uint32_t)( (wc.wr_id<<32)>>32);
//fprintf(stderr,"gen_wait_any() poped an events with cookie:%x\n",cookie);
		if (cookie != NULL_COOKIE) {
			verbs_req_t *req;
			void *test;

			dbg_info("removing event with cookie:%u", cookie);
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

	dbg_info("remaining: %d", htable_count(reqtable));

	if( __initialized <= 0 ) {
		log_err("Library not initialized.	Call photon_init() first");
		return -1;
	}

	if (ret_req == NULL || ret_proc == NULL) {
		return -1;
	}

	if (htable_count(ledger_reqtable) == 0) {
		log_err("No events on queue to wait_one()");
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

	dbg_info("completing ledger req %"PRIo32, cookie);
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

	dbg_info("completing event req %"PRIo32, cookie);
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

static void *__verbs_req_watcher(void *arg) {
	int i;
	int ne;
	int curr;
	uint32_t cookie;
	struct ibv_wc wc[32];

	dbg_info("reqs watcher started");

	if( __initialized == 0 ) {
		log_err("Library not initialized.	Call photon_init() first");
		pthread_exit((void*)-1);
	}

	// FIXME: Should run only when there's something to watch for (ledger/event).
	while(1) {
		// First we poll for CQEs and clear reqs waiting on them.
		// We don't want to spend too much time on this before moving to ledgers.
		ne = ibv_poll_cq(verbs_ctx.ib_cq, 32, wc);
		if (ne < 0) {
			log_err("poll CQ failed %d, EXITING WATCHER\n", ne);
			pthread_exit((void*)-1);
		}

		for (i = 0; i < ne; i++) {
			if (wc[i].status != IBV_WC_SUCCESS) {
				// TODO: is there anything we can/must do with the error?
				//	 I think the wr_id is valid, so we should probably notify the request.
				log_err("(status==%d) != IBV_WC_SUCCESS\n",wc[i].status);
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
				log_err("handshake_rdma_write_count is negative");
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
					log_err("couldn't find req for FIN ledger: %u", curr_entry->request);

				verbs_processes[i].local_FIN_ledger->curr = (verbs_processes[i].local_FIN_ledger->curr + 1) % verbs_processes[i].local_FIN_ledger->num_entries;
				dbg_info("%d requests left in reqtable", htable_count(ledger_reqtable));
			}
		}
	}

	pthread_exit(NULL);
}

#endif

///////////////////////////////////////////////////////////////////////////////
int verbs_wait_recv_buffer_rdma(int proc, int tag) {
	verbs_remote_buffer_t *curr_remote_buffer;
	verbs_ri_ledger_entry_t *curr_entry, *entry_iterator, tmp_entry;
	int count;
#ifdef DEBUG
	time_t stime;
#endif
	int curr, num_entries, still_searching;

	dbg_info("(%d, %d)", proc, tag);

	if( __initialized <= 0 ) {
		log_err("Library not initialized.	Call photon_init() first");
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

	dbg_info("Spinning on info ledger looking for receive request");
	dbg_info("curr == %d", verbs_processes[proc].local_rcv_info_ledger->curr);


	curr = verbs_processes[proc].local_rcv_info_ledger->curr;
	curr_entry = &(verbs_processes[proc].local_rcv_info_ledger->entries[curr]);

	dbg_info("looking in position %d/%p", verbs_processes[proc].local_rcv_info_ledger->curr, curr_entry);

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

	dbg_info("Request: %u", curr_entry->request);
	dbg_info("rkey: %u", curr_entry->rkey);
	dbg_info("Addr: %p", (void *)curr_entry->addr);
	dbg_info("Size: %u", curr_entry->size);
	dbg_info("Tag: %d",	curr_entry->tag);

	curr_entry->header = 0;
	curr_entry->footer = 0;

	num_entries = verbs_processes[proc].local_rcv_info_ledger->num_entries;
	curr = verbs_processes[proc].local_rcv_info_ledger->curr;
	curr = (curr + 1) % num_entries;
	verbs_processes[proc].local_rcv_info_ledger->curr = curr;

	dbg_info("new curr == %d", verbs_processes[proc].local_rcv_info_ledger->curr);

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

	dbg_info("(%d, %d)", proc, tag);

	if( __initialized <= 0 ) {
		log_err("Library not initialized.	Call photon_init() first");
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

	dbg_info("Spinning on info ledger looking for receive request");
	dbg_info("looking in position %d/%p", curr, curr_entry);

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

	dbg_info("Request: %u", curr_entry->request);
	dbg_info("Context: %u", curr_entry->rkey);
	dbg_info("Address: %p", (void *)curr_entry->addr);
	dbg_info("Size: %u", curr_entry->size);
	dbg_info("Tag: %d", curr_entry->tag);

	curr_entry->header = 0;
	curr_entry->footer = 0;

	num_entries = verbs_processes[proc].local_snd_info_ledger->num_entries;
	curr = verbs_processes[proc].local_snd_info_ledger->curr;
	curr = (curr + 1) % num_entries;
	verbs_processes[proc].local_snd_info_ledger->curr = curr;

	dbg_info("new curr == %d", verbs_processes[proc].local_snd_info_ledger->curr);

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

	dbg_info("(%d)", tag);

	if( __initialized <= 0 ) {
		log_err("Library not initialized.	 Call photon_init() first");
		goto error_exit;
	}

	dbg_info("Spinning on send info ledger looking for send request");

	still_searching = 1;
	iproc = -1;
#ifdef DEBUG
	stime = time(NULL);
#endif
	do {
		iproc = (iproc+1)%_photon_nproc;
		curr = verbs_processes[iproc].local_snd_info_ledger->curr;
		curr_entry = &(verbs_processes[iproc].local_snd_info_ledger->entries[curr]);
		dbg_info("looking in position %d/%p for proc %d", curr, curr_entry,iproc);

		count = 1;
		entry_iterator = curr_entry;
		// Some peers (procs) might have sent more than one send requests using different tags, so check them all.
		while(entry_iterator->header == 1 && entry_iterator->footer == 1) {
			if( (entry_iterator->addr == (uintptr_t)0) && (entry_iterator->rkey == 0) && ((tag < 0) || (entry_iterator->tag == tag )) ) {
				still_searching = 0;
				dbg_info("Found matching send request with tag %d from proc %d", tag, iproc);
				break;
			}
			else {
				dbg_info("Found non-matching send request with tag %d from proc %d", tag, iproc);
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

	dbg_info("new curr == %d", verbs_processes[iproc].local_snd_info_ledger->curr);

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

	dbg_info("(%d, %p, %u, %d, %p)", proc, ptr, size, tag, request);

	if( __initialized <= 0 ) {
		log_err("Library not initialized.	Call photon_init() first");
		return -1;
	}

	if (buffertable_find_containing( (void *) ptr, (int)size, &db) != 0) {
		log_err("Requested recv from ptr not in table");
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
				log_err("ibv_post_send() returned: %d and __verbs_wait_one() failed. Exiting",err);
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

		req = __verbs_get_request();
		if (!req) {
			log_err("Couldn't allocate request\n");
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

	dbg_info("New curr (proc=%d): %u", proc, verbs_processes[proc].remote_rcv_info_ledger->curr);

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

	dbg_info("(%d, %u, %d, %p)", proc, size, tag, request);

	if( __initialized <= 0 ) {
		log_err("Library not initialized.	 Call photon_init() first");
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
				log_err("ibv_post_send() returned: %d and __verbs_wait_one() failed. Exiting",err);
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

		req = __verbs_get_request();
		if (!req) {
			log_err("Couldn't allocate request\n");
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

		dbg_info("Inserting the RDMA request into the request table: %d/%p", request_id, req);

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
	dbg_info("New curr: %u", curr);

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

	dbg_info("(%d, %p, %u, %d, %p)", proc, ptr, size, tag, request);

	if( __initialized <= 0 ) {
		log_err("Library not initialized.	Call photon_init() first");
		goto error_exit;
	}

	if (buffertable_find_containing( (void *) ptr, (int)size, &db) != 0) {
		log_err("Requested post of send buffer for ptr not in table");
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
				log_err("ibv_post_send() returned: %d and __verbs_wait_one() failed. Exiting",err);
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

		req = __verbs_get_request();
		if (!req) {
			log_err("Couldn't allocate request\n");
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

		dbg_info("Inserting the RDMA request into the request table: %d/%p", request_id, req);

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
	dbg_info("New curr: %u", curr);

	return 0;

error_exit:
	if (request != NULL) {
		*request = NULL_COOKIE;
	}
	return -1;
}

///////////////////////////////////////////////////////////////////////////////
int verbs_post_os_put(int proc, char *ptr, uint32_t size, int tag, uint32_t remote_offset, uint32_t *request) {
	verbs_remote_buffer_t *drb;
	verbs_buffer_t *db;
	uint64_t cookie;
	int qp_index, err;
	uint32_t request_id;

	dbg_info("(%d, %p, %u, %u, %p)", proc, ptr, size, remote_offset, request);

	if( __initialized <= 0 ) {
		log_err("Library not initialized.	Call photon_init() first");
		return -1;
	}

	drb = verbs_processes[proc].curr_remote_buffer;

	if (drb->request == NULL_COOKIE) {
		log_err("Tried posting a send with no recv buffer. Have you called verbs_wait_recv_buffer_rdma() first?");
		return -1;
	}

	if (buffertable_find_containing( (void *)ptr, (int)size, &db) != 0) {
		log_err("Tried posting a send for a buffer not registered");
		return -1;
	}

	if (drb->size > 0 && size + remote_offset > drb->size) {
		log_err("Requested to send %u bytes to a %u buffer size at offset %u", size, drb->size, remote_offset);
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
				log_err("ibv_post_send() returned: %d and __verbs_wait_one() failed. Exiting",err);
				goto error_exit;
			}
#endif
		}
		while( err );
	}


	if (request != NULL) {
		verbs_req_t *req;

		*request = request_id;

		req = __verbs_get_request();
		if (!req) {
			log_err("Couldn't allocate request\n");
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
			log_err("Couldn't save request in hashtable");
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

	dbg_info("(%d, %p, %u, %u, %p)", proc, ptr, size, remote_offset, request);

	if( __initialized <= 0 ) {
		log_err("Library not initialized.	Call photon_init() first");
		return -1;
	}

	drb = verbs_processes[proc].curr_remote_buffer;

	if (drb->request == NULL_COOKIE) {
		log_err("Tried posting an os_get() with no send buffer");
		return -1;
	}

	if (buffertable_find_containing( (void *)ptr, (int)size, &db) != 0) {
		log_err("Tried posting a og_get() into a buffer that's not registered");
		return -1;
	}

	if ( (drb->size > 0) && ((size+remote_offset) > drb->size) ) {
		log_err("Requested to get %u bytes from a %u buffer size at offset %u", size, drb->size, remote_offset);
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
				log_err("ibv_post_send() returned: %d and __verbs_wait_one() failed. Exiting",err);
				goto error_exit;
			}
#endif
		}
		while( err );
	}

	if (request != NULL) {
		verbs_req_t *req;

		*request = request_id;

		req = __verbs_get_request();
		if (!req) {
			log_err("Couldn't allocate request\n");
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
			log_err("Couldn't save request in hashtable");
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

	dbg_info("(%d)", proc);

	if( __initialized <= 0 ) {
		log_err("Library not initialized.	 Call photon_init() first");
		return -1;
	}

	if (verbs_processes[proc].curr_remote_buffer->request == NULL_COOKIE) {
		log_err("Cannot send FIN, curr_remote_buffer->request is NULL_COOKIE");
		goto error_exit;
	}

	drb = verbs_processes[proc].curr_remote_buffer;
	curr = verbs_processes[proc].remote_FIN_ledger->curr;
	entry = &verbs_processes[proc].remote_FIN_ledger->entries[curr];
	dbg_info("verbs_processes[%d].remote_FIN_ledger->curr==%d",proc, curr);

	if( entry == NULL ) {
		log_err("entry is NULL for proc=%d",proc);
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
				log_err("returned: %d and __verbs_wait_one() failed. Exiting",err);
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

