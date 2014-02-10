#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <pthread.h>

#include "libphoton.h"
#include "photon_backend.h"
#include "photon_buffertable.h"
#include "photon_exchange.h"
#include "htable.h"
#include "counter.h"
#include "logging.h"
#include "squeue.h"

photonBuffer shared_storage;

static ProcessInfo *photon_processes;
static htable_t *reqtable, *ledger_reqtable;
static photonRequest requests;
static int num_requests;
static LIST_HEAD(freereqs, photon_req_t) free_reqs_list;
static LIST_HEAD(unreapedevdreqs, photon_req_t) unreaped_evd_reqs_list;
static LIST_HEAD(unreapedledgerreqs, photon_req_t) unreaped_ledger_reqs_list;
static LIST_HEAD(pendingreqs, photon_req_t) pending_reqs_list;
static SLIST_HEAD(pendingmemregs, photon_mem_register_req) pending_mem_register_list;

DEFINE_COUNTER(curr_cookie, uint32_t)
DEFINE_COUNTER(handshake_rdma_write, uint32_t)

/* default backend methods */
static int _photon_initialized(void);
static int _photon_init(photonConfig cfg, ProcessInfo *info, photonBuffer ss);
static int _photon_finalize(void);
static int _photon_register_buffer(void *buffer, uint64_t size);
static int _photon_unregister_buffer(void *buffer, uint64_t size);
static int _photon_test(uint32_t request, int *flag, int *type, photonStatus status);
static int _photon_wait(uint32_t request);
static int _photon_post_recv_buffer_rdma(int proc, char *ptr, uint32_t size, int tag, uint32_t *request);
static int _photon_post_send_buffer_rdma(int proc, char *ptr, uint32_t size, int tag, uint32_t *request);
static int _photon_post_send_request_rdma(int proc, uint32_t size, int tag, uint32_t *request);
static int _photon_wait_recv_buffer_rdma(int proc, int tag);
static int _photon_wait_send_buffer_rdma(int proc, int tag);
static int _photon_wait_send_request_rdma(int tag);
static int _photon_post_os_put(int proc, char *ptr, uint32_t size, int tag, uint32_t remote_offset, uint32_t *request);
static int _photon_post_os_get(int proc, char *ptr, uint32_t size, int tag, uint32_t remote_offset, uint32_t *request);
static int _photon_post_os_get_direct(int proc, void *ptr, uint64_t size, int tag, photonDescriptor rbuf, uint32_t *request);
static int _photon_send_FIN(int proc);
static int _photon_wait_any(int *ret_proc, uint32_t *ret_req);
static int _photon_wait_any_ledger(int *ret_proc, uint32_t *ret_req);
static int _photon_probe_ledger(int proc, int *flag, int type, photonStatus status);
static int _photon_io_init(char *file, int amode, MPI_Datatype view, int niter);
static int _photon_io_finalize();

static int __photon_nbpop_ledger(photonRequest req);
static int __photon_wait_ledger(photonRequest req);
static int __photon_wait_event(photonRequest req);
static int __photon_nbpop_event(photonRequest req);

static int _photon_rdma_put(int proc, uintptr_t laddr, uintptr_t raddr, uint64_t size,
							photonBuffer lbuf, photonRemoteBuffer rbuf, uint64_t id);
static int _photon_rdma_get(int proc, uintptr_t laddr, uintptr_t raddr, uint64_t size,
							photonBuffer lbuf, photonRemoteBuffer rbuf, uint64_t id);
static int _photon_rdma_send(int proc, uintptr_t laddr, uintptr_t raddr, uint64_t size,
							 photonBuffer lbuf, photonRemoteBuffer rbuf, uint64_t id);
static int _photon_rdma_recv(int proc, uintptr_t laddr, uintptr_t raddr, uint64_t size,
							 photonBuffer lbuf, photonRemoteBuffer rbuf, uint64_t id);
static int _photon_get_event(photonEventStatus stat);

/*
   We only want to spawn a dedicated thread for ledgers on
   multithreaded instantiations of the library (e.g. in xspd).
*/
#ifdef PHOTON_MULTITHREADED
static pthread_t ledger_watcher;
static void *__photon_req_watcher(void *arg);
#endif

struct photon_backend_t photon_default_backend = {
	.initialized = _photon_initialized,
	.init = _photon_init,
	.finalize = _photon_finalize,
	.register_buffer = _photon_register_buffer,
	.unregister_buffer = _photon_unregister_buffer,
	.test = _photon_test,
	.wait = _photon_wait,
	.wait_ledger = _photon_wait,
	.post_recv_buffer_rdma = _photon_post_recv_buffer_rdma,
	.post_send_buffer_rdma = _photon_post_send_buffer_rdma,
	.post_send_request_rdma = _photon_post_send_request_rdma,
	.wait_recv_buffer_rdma = _photon_wait_recv_buffer_rdma,
	.wait_send_buffer_rdma = _photon_wait_send_buffer_rdma,
	.wait_send_request_rdma = _photon_wait_send_request_rdma,
	.post_os_put = _photon_post_os_put,
	.post_os_get = _photon_post_os_get,
	.post_os_get_direct = _photon_post_os_get_direct,
	.send_FIN = _photon_send_FIN,
	.wait_any = _photon_wait_any,
	.wait_any_ledger = _photon_wait_any_ledger,
	.probe_ledger = _photon_probe_ledger,
	.io_init = _photon_io_init,
	.io_finalize = _photon_io_finalize,
	.rdma_get = _photon_rdma_get,
	.rdma_put = _photon_rdma_put,
	.rdma_send = _photon_rdma_send,
	.rdma_recv = _photon_rdma_recv,
	.get_event = _photon_get_event
};

static inline photonRequest __photon_get_request() {
	photonRequest req;

	LIST_LOCK(&free_reqs_list);
	req = LIST_FIRST(&free_reqs_list);
	if (req)
		LIST_REMOVE(req, list);
	LIST_UNLOCK(&free_reqs_list);

	if (!req) {
		req = malloc(sizeof(struct photon_req_t));
		pthread_mutex_init(&req->mtx, NULL);
		pthread_cond_init (&req->completed, NULL);
	}

	return req;
}

static int _photon_initialized() {
	if (__photon_backend && __photon_config)
		return __photon_backend->initialized();
	else
		return PHOTON_ERROR_NOINIT;
}

static int _photon_init(photonConfig cfg, ProcessInfo *info, photonBuffer ss) {
	int i, rc;
	char *buf;
	int bufsize, offset;
	int info_ledger_size, FIN_ledger_size;

	srand48(getpid() * time(NULL));
	
	dbg_info("(nproc %d, rank %d)",_photon_nproc, _photon_myrank);

	INIT_COUNTER(curr_cookie, 1);
	INIT_COUNTER(handshake_rdma_write, 0);

	requests = malloc(sizeof(struct photon_req_t) * DEF_NUM_REQUESTS);
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
		log_err("Failed to allocate buffer table");
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

	photon_processes = (ProcessInfo *) malloc(sizeof(ProcessInfo) * (_photon_nproc + _photon_nforw));
	if (!photon_processes) {
		log_err("Couldn't allocate process information");
		goto error_exit_lrt;
	}

	// Set it to zero, so that we know if it ever got initialized
	memset(photon_processes, 0, sizeof(ProcessInfo) * (_photon_nproc + _photon_nforw));

	for(i = 0; i < (_photon_nproc + _photon_nforw); i++) {
		photon_processes[i].curr_remote_buffer = photon_remote_buffer_create();
		if(!photon_processes[i].curr_remote_buffer) {
			log_err("Couldn't allocate process remote buffer information");
			goto error_exit_gp;
		}
	}

	dbg_info("alloc'd process info");

	// Everything is x2 cause we need a local and a remote copy of each ledger.
	// Remote Info (_ri_) ledger has an additional x2 cause we need "send-info" and "receive-info" ledgers.
	info_ledger_size = 2 * 2 * sizeof(struct photon_ri_ledger_entry_t) * LEDGER_SIZE * (_photon_nproc + _photon_nforw);
	FIN_ledger_size  = 2 * sizeof(struct photon_rdma_FIN_ledger_entry_t) * LEDGER_SIZE * (_photon_nproc + _photon_nforw);
	bufsize = info_ledger_size + FIN_ledger_size;
	buf = malloc(bufsize);
	if (!buf) {
		log_err("Couldn't allocate ledgers");
		goto error_exit_crb;
	}
	dbg_info("Bufsize: %d", bufsize);

	if (photon_setup_ri_ledgers(photon_processes, buf, LEDGER_SIZE) != 0) {
		log_err("couldn't setup snd/rcv info ledgers");
		goto error_exit_buf;
	}

	// skip 4 ledgers (rcv info local, rcv info remote, snd info local, snd info remote)
	offset = 4 * sizeof(struct photon_ri_ledger_entry_t) * LEDGER_SIZE * (_photon_nproc + _photon_nforw);
	if (photon_setup_FIN_ledger(photon_processes, buf + offset, LEDGER_SIZE) != 0) {
		log_err("couldn't setup send ledgers");
		goto error_exit_buf;
	}

	shared_storage = photon_buffer_create(buf, bufsize);
	if (!shared_storage) {
		log_err("Couldn't register shared storage");
		goto error_exit_buf;
	}

	rc = __photon_backend->init(cfg, photon_processes, shared_storage);
	if (rc != PHOTON_OK) {
		log_err("Could not initialize photon backend");
		goto error_exit_ss;
	}

	/* register any buffers that were requested before init */
	while( !SLIST_EMPTY(&pending_mem_register_list) ) {
		struct photon_mem_register_req *mem_reg_req;
		dbg_info("registering buffer in queue");
		mem_reg_req = SLIST_FIRST(&pending_mem_register_list);
		SLIST_REMOVE_HEAD(&pending_mem_register_list, list);
		photon_register_buffer(mem_reg_req->buffer, mem_reg_req->buffer_size);
	}

#ifdef PHOTON_MULTITHREADED
	if (pthread_create(&ledger_watcher, NULL, __photon_req_watcher, NULL)) {
		log_err("pthread_create() failed");
		goto error_exit_ss;
	}
#endif
	
	dbg_info("ended successfully =============");

	return PHOTON_OK;

error_exit_ss:
	photon_buffer_free(shared_storage);
error_exit_buf:
	if (buf)
		free(buf);
error_exit_crb:
	for(i = 0; i < (_photon_nproc + _photon_nforw); i++) {
		if (photon_processes[i].curr_remote_buffer != NULL) {
			photon_remote_buffer_free(photon_processes[i].curr_remote_buffer);
		}
	}
error_exit_gp:
	free(photon_processes);
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

	return PHOTON_ERROR;
}

static int _photon_finalize() {
	int rc;

	rc = __photon_backend->finalize();
	if (rc != PHOTON_OK) {
		log_err("Could not finalize backend");
		return PHOTON_ERROR;
	}
	return PHOTON_OK;
}

static int _photon_register_buffer(void *buffer, uint64_t size) {
	static int first_time = 1;
	photonBuffer db;
	
	dbg_info("(%p, %lu)", buffer, size);

	if(__photon_backend->initialized() != PHOTON_OK) {
		struct photon_mem_register_req *mem_reg_req;
		if( first_time ) {
			SLIST_INIT(&pending_mem_register_list);
			first_time = 0;
		}
		mem_reg_req = malloc( sizeof(struct photon_mem_register_req) );
		mem_reg_req->buffer = buffer;
		mem_reg_req->buffer_size = size;

		SLIST_INSERT_HEAD(&pending_mem_register_list, mem_reg_req, list);
		dbg_info("called before init, queueing buffer info");
		goto normal_exit;
	}

	if (buffertable_find_exact(buffer, size, &db) == 0) {
		dbg_info("we had an existing buffer, reusing it");
		db->ref_count++;
		goto normal_exit;
	}

	db = photon_buffer_create(buffer, size);
	if (!db) {
		log_err("Couldn't register shared storage");
		goto error_exit;
	}

	dbg_info("created buffer: %p", db);

	if (photon_buffer_register(db, __photon_backend->context) != 0) {
		log_err("Couldn't register buffer");
		goto error_exit_db;
	}

	dbg_info("registered buffer");

	if (buffertable_insert(db) != 0) {
		goto error_exit_db;
	}

	dbg_info("added buffer to hash table");

normal_exit:
	return PHOTON_OK;
error_exit_db:
	photon_buffer_free(db);
error_exit:
	return PHOTON_ERROR;
}

static int _photon_unregister_buffer(void *buffer, uint64_t size) {
	photonBuffer db;

	dbg_info();

	if(__photon_backend->initialized() != PHOTON_OK) {
		init_err();
		return PHOTON_ERROR_NOINIT;
	}

	if (buffertable_find_exact(buffer, size, &db) != 0) {
		dbg_info("no such buffer is registered");
		goto error_exit;
	}

	if (--(db->ref_count) == 0) {
		if (photon_buffer_unregister(db, __photon_backend->context) != 0) {
			goto error_exit;
		}
		buffertable_remove(db);
		photon_buffer_free(db);
	}

	return PHOTON_OK;

error_exit:
	return PHOTON_ERROR;
}

///////////////////////////////////////////////////////////////////////////////
// __photon_nbpop_event() is non blocking and returns:
// -1 if an error occured.
//	0 if the request (req) specified in the argument has completed.
//	1 if either no event was in the queue, or there was an event but not for the specified request (req).
static int __photon_nbpop_event(photonRequest req) {
	int rc;

	dbg_info("(%d)",req->id);

	if(req->state == REQUEST_PENDING) {
		uint32_t cookie;
		photon_event_status event;
		
		rc = __photon_backend->get_event(&event);
		if (rc != PHOTON_OK) {
			dbg_err("Could not get event");
			goto error_exit;
		}

		cookie = (uint32_t)( (event.id<<32)>>32);
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
				photonRequest tmp_req = test;

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
// returns
// -1 if an error occured.
//	0 if the FIN associated with "req" was found and poped, or
//		the "req" is not pending.	 This is not an error, if a previous
//		call to __photon_nbpop_ledger() poped the FIN that corresponds to "req".
//	1 if the request is pending and the FIN has not arrived yet
static int __photon_nbpop_ledger(photonRequest req) {
	void *test;
	int curr, i=-1;

	dbg_info("(%d)",req->id);

	//#ifdef DEBUG
	//		for(i = 0; i < _photon_nproc; i++) {
	//				photon_rdma_FIN_ledger_entry_t *curr_entry;
	//				curr = photon_processes[i].local_FIN_ledger->curr;
	//				curr_entry = &(photon_processes[i].local_FIN_ledger->entries[curr]);
	//				dbg_info("__photon_nbpop_ledger() curr_entry(proc==%d)=%p",i,curr_entry);
	//		}
	//#endif

	if(req->state == REQUEST_PENDING) {

		// Check if an entry of the FIN LEDGER was written with "id" equal to to "req"
		for(i = 0; i < _photon_nproc; i++) {
			photonFINLedgerEntry curr_entry;
			curr = photon_processes[i].local_FIN_ledger->curr;
			curr_entry = &(photon_processes[i].local_FIN_ledger->entries[curr]);
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
					int num = photon_processes[i].local_FIN_ledger->num_entries;
					int new_curr = (photon_processes[i].local_FIN_ledger->curr + 1) % num;
					photon_processes[i].local_FIN_ledger->curr = new_curr;
					dbg_info("returning 0");
					return 0;
				}
				else {
					photonRequest tmp_req;

					if (htable_lookup(ledger_reqtable, (uint64_t)curr_entry->request, &test) == 0) {
						tmp_req = test;

						tmp_req->state = REQUEST_COMPLETED;
						LIST_REMOVE(tmp_req, list);
						LIST_INSERT_HEAD(&unreaped_ledger_reqs_list, tmp_req, list);
					}
				}

				int num = photon_processes[i].local_FIN_ledger->num_entries;
				int new_curr = (photon_processes[i].local_FIN_ledger->curr + 1) % num;
				photon_processes[i].local_FIN_ledger->curr = new_curr;
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

static int __photon_wait_ledger(photonRequest req) {
	void *test;
	int curr, num_entries, i=-1;

	dbg_info("(%d)",req->id);

#ifdef DEBUG
	for(i = 0; i < _photon_nproc; i++) {
		photonFINLedgerEntry curr_entry;
		curr = photon_processes[i].local_FIN_ledger->curr;
		curr_entry = &(photon_processes[i].local_FIN_ledger->entries[curr]);
		dbg_info("curr_entry(proc==%d)=%p",i,curr_entry);
	}
#endif
	while(req->state == REQUEST_PENDING) {

		// Check if an entry of the FIN LEDGER was written with "id" equal to to "req"
		for(i = 0; i < _photon_nproc; i++) {
			photonFINLedgerEntry curr_entry;
			curr = photon_processes[i].local_FIN_ledger->curr;
			curr_entry = &(photon_processes[i].local_FIN_ledger->entries[curr]);
			if (curr_entry->header != (uint8_t) 0 && curr_entry->footer != (uint8_t) 0) {
				dbg_info("Found: %d/%u/%u", curr, curr_entry->request, req->id);
				curr_entry->header = 0;
				curr_entry->footer = 0;

				if (curr_entry->request == req->id) {
					req->state = REQUEST_COMPLETED;
				}
				else {
					photonRequest tmp_req;

					if (htable_lookup(ledger_reqtable, (uint64_t)curr_entry->request, &test) == 0) {
						tmp_req = test;

						tmp_req->state = REQUEST_COMPLETED;
						SAFE_LIST_REMOVE(tmp_req, list);
						SAFE_LIST_INSERT_HEAD(&unreaped_ledger_reqs_list, tmp_req, list);
					}
				}

				num_entries = photon_processes[i].local_FIN_ledger->num_entries;
				curr = photon_processes[i].local_FIN_ledger->curr;
				curr = (curr + 1) % num_entries;
				photon_processes[i].local_FIN_ledger->curr = curr;
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

static int __photon_wait_event(photonRequest req) {
	//fprintf(stderr,"[%d/%d] __photon_wait_event(): req->state:%d, req->id:%d\n", _photon_myrank, _photon_nproc, req->state, req->id);

	// I think here we should check if the request is in the unreaped_evd_reqs_list
	// (i.e. already completed) and if so, move it to the free_reqs_list:
	//		if(req->state == REQUEST_COMPLETED){
	//				LIST_REMOVE(req, list);
	//				LIST_INSERT_HEAD(&free_reqs_list, req, list);
	//		}
	int rc;

	while(req->state == REQUEST_PENDING) {
		uint32_t cookie;
		photon_event_status event;
		
		rc = __photon_backend->get_event(&event);
		if (rc != PHOTON_OK) {
			dbg_err("Could not get event");
			goto error_exit;
		}		

		cookie = (uint32_t)( (event.id<<32)>>32);
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
				photonRequest tmp_req = test;

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
// photon_test() is a nonblocking operation that checks the event queue to see if
// the event associated with the "request" parameter has completed.	 It returns:
//	0 if the event associated with "request" was in the queue and was successfully poped.
//	1 if "request" was not in the request tables.	 This is not an error if photon_test()
//		is called in a loop and is called multiple times for each request.
// -1 if an error occured.
//
// When photon_test() returns zero (success) the "flag" parameter has the value:
//	0 if the event that was poped does not correspond to "request", or if none of the operations completed.
//	1 if the event that was poped does correspond to "request".
//
//	When photon_test() returns 0 and flag==0 the "status" structure is also filled
//	unless the constant "MPI_STATUS_IGNORE" was passed as the "status" argument.
//
// Regardless of the return value and the value of "flag", the parameter "type"
// will be set to 0 (zero) when the request is of type event and 1 (one) when the
// request is of type ledger.
static int _photon_test(uint32_t request, int *flag, int *type, photonStatus status) {
	photonRequest req;
	void *test;
	int ret_val;

	dbg_info("(%d)",request);

	if (htable_lookup(reqtable, (uint64_t)request, &test) != 0) {
		if (htable_lookup(ledger_reqtable, (uint64_t)request, &test) != 0) {
			dbg_info("Request is not in either request-table");
			// Unlike photon_wait(), we might call photon_test() multiple times on a request,
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
		ret_val = __photon_nbpop_ledger(req);
	}
	else {
		if( type != NULL ) *type = 0;
		ret_val = __photon_nbpop_event(req);
	}
#endif

	if( !ret_val ) {
		*flag = 1;
		status->src_addr = req->proc;
		status->tag = req->tag;
		status->count = 1;
		status->error = 0;
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

static int _photon_wait(uint32_t request) {
	photonRequest req;

	dbg_info("(%d)",request);

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
		return __photon_wait_ledger(req);
	else
		return __photon_wait_event(req);
#endif
}

static int _photon_post_recv_buffer_rdma(int proc, char *ptr, uint32_t size, int tag, uint32_t *request) {
	photonBuffer db;
	uint64_t cookie;
	photonRILedgerEntry entry;
	int curr, num_entries, rc;
	uint32_t request_id;

	dbg_info("(%d, %p, %u, %d, %p)", proc, ptr, size, tag, request);

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
		proc = photon_wait_send_request_rdma(tag);
	}

	curr = photon_processes[proc].remote_rcv_info_ledger->curr;
	entry = &photon_processes[proc].remote_rcv_info_ledger->entries[curr];

	// fill in what we're going to transfer
	entry->header = 1;
	entry->request = request_id;
	if (db->mr)
		entry->rkey = db->mr->rkey;
	entry->addr = (uintptr_t) ptr;
	entry->size = size;
	entry->tag = tag;
	entry->footer = 1;
#ifdef HAVE_UGNI
	entry->qword1 = db->mdh.qword1;
	entry->qword2 = db->mdh.qword2;
#endif

	dbg_info("Post recv");
	dbg_info("Request: %u", entry->request);
	dbg_info("rkey: %u", entry->rkey);
	dbg_info("Address: %p", (void *)entry->addr);
	dbg_info("Size: %u", entry->size);
	dbg_info("Tag: %d", entry->tag);
#ifdef HAVE_UGNI
	dbg_info("MDH: 0x%016lx / 0x%016lx", entry->qword1, entry->qword2);
#endif

	{
		uintptr_t rmt_addr;
		rmt_addr  = photon_processes[proc].remote_rcv_info_ledger->remote.addr;
		rmt_addr += photon_processes[proc].remote_rcv_info_ledger->curr * sizeof(*entry);
		cookie = (( (uint64_t)proc)<<32) | NULL_COOKIE;
		
		rc = __photon_backend->rdma_put(proc, (uintptr_t)entry, rmt_addr, sizeof(*entry), shared_storage,
						&(photon_processes[proc].remote_rcv_info_ledger->remote), cookie);
		if (rc != PHOTON_OK) {
			dbg_err("RDMA PUT failed for %lu\n", cookie);
			goto error_exit;
		}
	}

	// TODO: I don't think that this is a sufficient solution.
	// Keep track of the dat_ep_post_rdma_write()s that do not insert an entry into "reqtable", so
	// that __photon_wait_one() is able to wait on this event's completion
	INC_COUNTER(handshake_rdma_write);

	if (request != NULL) {
		photonRequest req;

		req = __photon_get_request();
		if (!req) {
			log_err("Couldn't allocate request\n");
			goto error_exit;
		}
		req->id = request_id;
		req->state = REQUEST_PENDING;
		// photon_post_recv_buffer_rdma() initiates a receiver initiated handshake.	For this reason,
		// we don't care when the function is completed, but rather when the transfer associated with
		// this handshake is completed.	 This will be reflected in the LEDGER by the corresponding
		// photon_send_FIN() posted by the sender.
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

	num_entries = photon_processes[proc].remote_rcv_info_ledger->num_entries;
	curr = photon_processes[proc].remote_rcv_info_ledger->curr;
	curr = (curr + 1) % num_entries;
	photon_processes[proc].remote_rcv_info_ledger->curr = curr;

	dbg_info("New curr (proc=%d): %u", proc, photon_processes[proc].remote_rcv_info_ledger->curr);

	return PHOTON_OK;

error_exit:
	if (request != NULL) {
		*request = NULL_COOKIE;
	}
	return PHOTON_ERROR;
}

static int _photon_post_send_buffer_rdma(int proc, char *ptr, uint32_t size, int tag, uint32_t *request) {
	photonBuffer db;
	photonRILedgerEntry entry;
	int curr, num_entries, rc;
	uint64_t cookie;
	uint32_t request_id;

	dbg_info("(%d, %p, %u, %d, %p)", proc, ptr, size, tag, request);

	if (buffertable_find_containing( (void *) ptr, (int)size, &db) != 0) {
		log_err("Requested post of send buffer for ptr not in table");
		goto error_exit;
	}

	request_id = INC_COUNTER(curr_cookie);
	dbg_info("Incrementing curr_cookie_count to: %d", request_id);

	curr = photon_processes[proc].remote_snd_info_ledger->curr;
	entry = &photon_processes[proc].remote_snd_info_ledger->entries[curr];

	// fill in what we're going to transfer
	entry->header = 1;
	entry->request = request_id;
	if (db->mr)
		entry->rkey = db->mr->rkey;
	entry->addr = (uintptr_t)ptr;
	entry->size = size;
	entry->tag = tag;
	entry->footer = 1;
#ifdef HAVE_UGNI
	entry->qword1 = db->mdh.qword1;
	entry->qword2 = db->mdh.qword2;
#endif

	dbg_info("Post send request");
	dbg_info("Request: %u", entry->request);
	dbg_info("rkey: %u", entry->rkey);
	dbg_info("Addr: %p", (void *)entry->addr);
	dbg_info("Size: %u", entry->size);
	dbg_info("Tag: %d", entry->tag);
#ifdef HAVE_UGNI
	dbg_info("MDH: 0x%016lx / 0x%016lx", entry->qword1, entry->qword2);
#endif
	{
		uintptr_t rmt_addr;
		rmt_addr  = photon_processes[proc].remote_snd_info_ledger->remote.addr;
		rmt_addr += photon_processes[proc].remote_snd_info_ledger->curr * sizeof(*entry);
		cookie = (( (uint64_t)proc)<<32) | request_id;		

		rc = __photon_backend->rdma_put(proc, (uintptr_t)entry, rmt_addr, sizeof(*entry), shared_storage,
						&(photon_processes[proc].remote_rcv_info_ledger->remote), cookie);
		if (rc != PHOTON_OK) {
			dbg_err("RDMA PUT failed for %lu\n", cookie);
			goto error_exit;
		}
	}

	// Keep track of the dat_ep_post_rdma_write()s that do not insert an entry into "reqtable", so
	// that __photon_wait_one() is able to wait on this event's completion
	INC_COUNTER(handshake_rdma_write);

	if (request != NULL) {
		photonRequest req;

		req = __photon_get_request();
		if (!req) {
			log_err("Couldn't allocate request\n");
			goto error_exit;
		}
		req->id = request_id;
		req->state = REQUEST_PENDING;
		// photon_post_send_buffer_rdma() initiates a sender initiated handshake.	For this reason,
		// we don't care when the function is completed, but rather when the transfer associated with
		// this handshake is completed.	 This will be reflected in the LEDGER by the corresponding
		// photon_send_FIN() posted by the receiver.
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

	num_entries = photon_processes[proc].remote_snd_info_ledger->num_entries;
	curr = photon_processes[proc].remote_snd_info_ledger->curr;
	curr = (curr + 1) % num_entries;
	photon_processes[proc].remote_snd_info_ledger->curr = curr;
	dbg_info("New curr: %u", curr);

	return PHOTON_OK;

error_exit:
	if (request != NULL) {
		*request = NULL_COOKIE;
	}
	return PHOTON_ERROR;
}

static int _photon_post_send_request_rdma(int proc, uint32_t size, int tag, uint32_t *request) {
	photonRILedgerEntry entry;
	int curr, num_entries, rc;
	uint64_t cookie;
	uint32_t request_id;

	dbg_info("(%d, %u, %d, %p)", proc, size, tag, request);

	request_id = INC_COUNTER(curr_cookie);
	dbg_info("Incrementing curr_cookie_count to: %d", request_id);

	curr = photon_processes[proc].remote_snd_info_ledger->curr;
	entry = &photon_processes[proc].remote_snd_info_ledger->entries[curr];

	// fill in what we're going to transfer
	entry->header = 1;
	entry->request = request_id;
	entry->rkey = 0;	      // We are not really giving our peer information about some
	entry->addr = (uintptr_t)0;   // send buffer here.	Just our intention to send() in the future.
	entry->size = size;
	entry->tag = tag;
	entry->footer = 1;
#ifdef HAVE_UGNI
	entry->qword1 = 0;
	entry->qword2 = 0;
#endif

	dbg_info("Post send request");
	dbg_info("Request: %u", entry->request);
	dbg_info("rkey: %u", entry->rkey);
	dbg_info("Addr: %p", (void *)entry->addr);
	dbg_info("Size: %u", entry->size);
	dbg_info("Tag: %d", entry->tag);
#ifdef HAVE_UGNI
	dbg_info("MDH: 0x%016lx / 0x%016lx", entry->qword1, entry->qword2);
#endif
	{
		uintptr_t rmt_addr;
		rmt_addr  = photon_processes[proc].remote_snd_info_ledger->remote.addr;
		rmt_addr += photon_processes[proc].remote_snd_info_ledger->curr * sizeof(*entry);
		cookie = (( (uint64_t)proc)<<32) | request_id;		

		rc = __photon_backend->rdma_put(proc, (uintptr_t)entry, rmt_addr, sizeof(*entry), shared_storage,
						&(photon_processes[proc].remote_rcv_info_ledger->remote), cookie);
		if (rc != PHOTON_OK) {
			dbg_err("RDMA PUT failed for %lu\n", cookie);
			goto error_exit;
		}
	}

	// Keep track of the dat_ep_post_rdma_write()s that do not insert an entry into "reqtable", so
	// that __photon_wait_one() is able to wait on this event's completion
	INC_COUNTER(handshake_rdma_write);

	if (request != NULL) {
		photonRequest req;

		req = __photon_get_request();
		if (!req) {
			log_err("Couldn't allocate request\n");
			goto error_exit;
		}
		req->id = request_id;
		req->state = REQUEST_PENDING;
		// photon_post_send_request_rdma() causes an RDMA transfer, but its own completion is
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

	num_entries = photon_processes[proc].remote_snd_info_ledger->num_entries;
	curr = photon_processes[proc].remote_snd_info_ledger->curr;
	curr = (curr + 1) % num_entries;
	photon_processes[proc].remote_snd_info_ledger->curr = curr;
	dbg_info("New curr: %u", curr);

	return PHOTON_OK;

error_exit:
	if (request != NULL) {
		*request = NULL_COOKIE;
	}
	return PHOTON_ERROR;
}

static int _photon_wait_recv_buffer_rdma(int proc, int tag) {
	photonRemoteBuffer curr_remote_buffer;
	photonRILedgerEntry curr_entry, entry_iterator;
	struct photon_ri_ledger_entry_t tmp_entry;
	int count;
#ifdef DEBUG
	time_t stime;
#endif
	int curr, num_entries, still_searching;

	dbg_info("(%d, %d)", proc, tag);

	// If we've received a Rendezvous-Start from processor "proc" that is still pending
	curr_remote_buffer = photon_processes[proc].curr_remote_buffer;
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
	dbg_info("curr == %d", photon_processes[proc].local_rcv_info_ledger->curr);

	curr = photon_processes[proc].local_rcv_info_ledger->curr;
	curr_entry = &(photon_processes[proc].local_rcv_info_ledger->entries[curr]);

	dbg_info("looking in position %d/%p", photon_processes[proc].local_rcv_info_ledger->curr, curr_entry);

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
			curr = photon_processes[proc].local_rcv_info_ledger->curr;
			num_entries = photon_processes[proc].local_rcv_info_ledger->num_entries;
			curr = (curr + count++) % num_entries;
			entry_iterator = &(photon_processes[proc].local_rcv_info_ledger->entries[curr]);
		}
	}
	while(still_searching);

	// If it wasn't the first pending receive request, swap the one we will serve ( entry_iterator) with
	// the first pending (curr_entry) in the info ledger, so that we can increment the current pointer
	// (photon_processes[proc].local_rcv_info_ledger->curr) and skip the request we will serve without losing any
	// pending requests.
	if( entry_iterator != curr_entry ) {
		tmp_entry = *entry_iterator;
		*entry_iterator = *curr_entry;
		*curr_entry = tmp_entry;
	}

	photon_processes[proc].curr_remote_buffer->request = curr_entry->request;
	photon_processes[proc].curr_remote_buffer->rkey = curr_entry->rkey;
	photon_processes[proc].curr_remote_buffer->addr = curr_entry->addr;
	photon_processes[proc].curr_remote_buffer->size = curr_entry->size;
	photon_processes[proc].curr_remote_buffer->tag	 = curr_entry->tag;
#ifdef HAVE_UGNI
	photon_processes[proc].curr_remote_buffer->mdh.qword1  = curr_entry->qword1;
	photon_processes[proc].curr_remote_buffer->mdh.qword2  = curr_entry->qword2;
#endif

	dbg_info("Request: %u", curr_entry->request);
	dbg_info("rkey: %u", curr_entry->rkey);
	dbg_info("Addr: %p", (void *)curr_entry->addr);
	dbg_info("Size: %u", curr_entry->size);
	dbg_info("Tag: %d",	curr_entry->tag);
#ifdef HAVE_UGNI
	dbg_info("MDH: 0x%016lx / 0x%016lx", curr_entry->qword1, curr_entry->qword2);
#endif

	curr_entry->header = 0;
	curr_entry->footer = 0;

	num_entries = photon_processes[proc].local_rcv_info_ledger->num_entries;
	curr = photon_processes[proc].local_rcv_info_ledger->curr;
	curr = (curr + 1) % num_entries;
	photon_processes[proc].local_rcv_info_ledger->curr = curr;

	dbg_info("new curr == %d", photon_processes[proc].local_rcv_info_ledger->curr);

normal_exit:
	return PHOTON_OK;
error_exit:
	return PHOTON_ERROR;
}

// photon_wait_send_buffer_rdma() should never be called between a photon_wait_recv_buffer_rdma()
// and the corresponding photon_post_os_put(), or between an other photon_wait_send_buffer_rdma()
// and the corresponding photon_post_os_get() for the same proc.
// In other words if photon_processes[proc].curr_remote_buffer is full, photon_wait_send_buffer_rdma()
// should not be called.
static int _photon_wait_send_buffer_rdma(int proc, int tag) {
	photonRemoteBuffer curr_remote_buffer;
	photonRILedgerEntry curr_entry, entry_iterator;
	struct photon_ri_ledger_entry_t tmp_entry;
	int count;
#ifdef DEBUG
	time_t stime;
#endif
	int curr, num_entries, still_searching;

	dbg_info("(%d, %d)", proc, tag);

	// If we've received a Rendezvous-Start from processor "proc" that is still pending
	curr_remote_buffer = photon_processes[proc].curr_remote_buffer;
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

	curr = photon_processes[proc].local_snd_info_ledger->curr;
	curr_entry = &(photon_processes[proc].local_snd_info_ledger->entries[curr]);

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
			curr = (photon_processes[proc].local_snd_info_ledger->curr + count++) % photon_processes[proc].local_snd_info_ledger->num_entries;
			entry_iterator = &(photon_processes[proc].local_snd_info_ledger->entries[curr]);
		}
	}
	while(still_searching);

	// If it wasn't the first pending receive request, swap the one we will serve (entry_iterator) with
	// the first pending (curr_entry) in the info ledger, so that we can increment the current pointer
	// (photon_processes[proc].local_snd_info_ledger->curr) and skip the request we will serve without losing any
	// pending requests.
	if( entry_iterator != curr_entry ) {
		tmp_entry = *entry_iterator;
		*entry_iterator = *curr_entry;
		*curr_entry = tmp_entry;
	}

	photon_processes[proc].curr_remote_buffer->request = curr_entry->request;
	photon_processes[proc].curr_remote_buffer->rkey = curr_entry->rkey;
	photon_processes[proc].curr_remote_buffer->addr = curr_entry->addr;
	photon_processes[proc].curr_remote_buffer->size = curr_entry->size;
	photon_processes[proc].curr_remote_buffer->tag = curr_entry->tag;
#ifdef HAVE_UGNI
	photon_processes[proc].curr_remote_buffer->mdh.qword1  = curr_entry->qword1;
	photon_processes[proc].curr_remote_buffer->mdh.qword2  = curr_entry->qword2;
#endif

	dbg_info("Request: %u", curr_entry->request);
	dbg_info("Context: %u", curr_entry->rkey);
	dbg_info("Address: %p", (void *)curr_entry->addr);
	dbg_info("Size: %u", curr_entry->size);
	dbg_info("Tag: %d", curr_entry->tag);
#ifdef HAVE_UGNI
	dbg_info("MDH: 0x%016lx / 0x%016lx", curr_entry->qword1, curr_entry->qword2);
#endif

	curr_entry->header = 0;
	curr_entry->footer = 0;

	num_entries = photon_processes[proc].local_snd_info_ledger->num_entries;
	curr = photon_processes[proc].local_snd_info_ledger->curr;
	curr = (curr + 1) % num_entries;
	photon_processes[proc].local_snd_info_ledger->curr = curr;

	dbg_info("new curr == %d", photon_processes[proc].local_snd_info_ledger->curr);
	
 normal_exit:
	return PHOTON_OK;
 error_exit:
	return PHOTON_ERROR;
}

static int _photon_wait_send_request_rdma(int tag) {
	photonRILedgerEntry curr_entry, entry_iterator;
	struct photon_ri_ledger_entry_t tmp_entry;
	int count, iproc;
#ifdef DEBUG
	time_t stime;
#endif
	int curr, num_entries, still_searching;

	dbg_info("(%d)", tag);

	dbg_info("Spinning on send info ledger looking for send request");

	still_searching = 1;
	iproc = -1;
#ifdef DEBUG
	stime = time(NULL);
#endif
	do {
		iproc = (iproc+1)%_photon_nproc;
		curr = photon_processes[iproc].local_snd_info_ledger->curr;
		curr_entry = &(photon_processes[iproc].local_snd_info_ledger->entries[curr]);
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
				curr = (photon_processes[iproc].local_snd_info_ledger->curr + count) % photon_processes[iproc].local_snd_info_ledger->num_entries;
				++count;
				entry_iterator = &(photon_processes[iproc].local_snd_info_ledger->entries[curr]);
			}
		}
#ifdef DEBUG
		stime = _tictoc(stime, -1);
#endif
	}
	while(still_searching);

	// If it wasn't the first pending send request, swap the one we will serve (entry_iterator) with
	// the first pending (curr_entry) in the send info ledger, so that we can increment the current pointer
	// (photon_processes[iproc].local_snd_info_ledger->curr) and skip the request we will serve without losing any
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

	num_entries = photon_processes[iproc].local_snd_info_ledger->num_entries;
	curr = photon_processes[iproc].local_snd_info_ledger->curr;
	curr = (curr + 1) % num_entries;
	photon_processes[iproc].local_snd_info_ledger->curr = curr;

	dbg_info("new curr == %d", photon_processes[iproc].local_snd_info_ledger->curr);

	return PHOTON_OK;
}

static int _photon_post_os_put(int proc, char *ptr, uint32_t size, int tag, uint32_t remote_offset, uint32_t *request) {
	photonRemoteBuffer drb;
	photonBuffer db;
	uint64_t cookie;
	int rc;
	uint32_t request_id;

	dbg_info("(%d, %p, %u, %u, %p)", proc, ptr, size, remote_offset, request);

	drb = photon_processes[proc].curr_remote_buffer;

	if (drb->request == NULL_COOKIE) {
		log_err("Tried posting a send with no recv buffer. Have you called photon_wait_recv_buffer_rdma() first?");
		goto error_exit;
	}

	if (buffertable_find_containing( (void *)ptr, (int)size, &db) != 0) {
		log_err("Tried posting a send for a buffer not registered");
		goto error_exit;
	}

	if (drb->size > 0 && size + remote_offset > drb->size) {
		log_err("Requested to send %u bytes to a %u buffer size at offset %u", size, drb->size, remote_offset);
		goto error_exit;
	}

	request_id = INC_COUNTER(curr_cookie);
	dbg_info("Incrementing curr_cookie_count to: %d", request_id);

	cookie = (( (uint64_t)proc)<<32) | request_id;
	dbg_info("Posted Cookie: %u/%u/%"PRIo64, proc, request_id, cookie);

	{
	  rc = __photon_backend->rdma_put(proc, (uintptr_t)ptr, drb->addr + (uintptr_t)remote_offset,
					  size, db, drb, cookie);
	  
	  if (rc != PHOTON_OK) {
	    dbg_err("RDMA PUT failed for %lu\n", cookie);
	    goto error_exit;
	  }		
	}

	if (request != NULL) {
		photonRequest req;

		*request = request_id;

		req = __photon_get_request();
		if (!req) {
			log_err("Couldn't allocate request\n");
			goto error_exit;
		}
		req->id = request_id;
		req->state = REQUEST_PENDING;
		// photon_post_os_put() causes an RDMA transfer, but its own completion is
		// communicated to the task that posts it through a completion event.
		req->type = EVQUEUE;
		req->proc = proc;
		req->tag = tag;
		SAFE_LIST_INSERT_HEAD(&pending_reqs_list, req, list);

		dbg_info("Inserting the OS send request into the request table: %d/%d/%p", proc, request_id, req);

		if (htable_insert(reqtable, (uint64_t)request_id, req) != 0) {
			// this is bad, we've submitted the request, but we can't track it
			log_err("Couldn't save request in hashtable");
			goto error_exit;
		}
	}

	return PHOTON_OK;

error_exit:
	if (request != NULL) {
		*request = NULL_COOKIE;
	}
	return PHOTON_ERROR;
}

static int _photon_post_os_get(int proc, char *ptr, uint32_t size, int tag, uint32_t remote_offset, uint32_t *request) {
	photonRemoteBuffer drb;
	photonBuffer db;
	uint64_t cookie;
	int rc;
	uint32_t request_id;

	dbg_info("(%d, %p, %u, %u, %p)", proc, ptr, size, remote_offset, request);

	drb = photon_processes[proc].curr_remote_buffer;

	if (drb->request == NULL_COOKIE) {
		log_err("Tried posting an os_get() with no send buffer");
		return -1;
	}

	if (buffertable_find_containing( (void *)ptr, (int)size, &db) != 0) {
		log_err("Tried posting a os_get() into a buffer that's not registered");
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

		rc = __photon_backend->rdma_get(proc, (uintptr_t)ptr, drb->addr + (uintptr_t)remote_offset,
						size, db, drb, cookie);
		
		if (rc != PHOTON_OK) {
			dbg_err("RDMA GET failed for %lu\n", cookie);
			goto error_exit;
		}
	}

	if (request != NULL) {
		photonRequest req;

		*request = request_id;

		req = __photon_get_request();
		if (!req) {
			log_err("Couldn't allocate request\n");
			goto error_exit;
		}
		req->id = request_id;
		req->state = REQUEST_PENDING;
		// photon_post_os_get() causes an RDMA transfer, but its own completion is
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

	return PHOTON_OK;

error_exit:
	if (request != NULL) {
		*request = NULL_COOKIE;
	}
	return PHOTON_ERROR;
}

static int _photon_post_os_get_direct(int proc, void *ptr, uint64_t size, int tag, photonDescriptor rbuf, uint32_t *request) {
	photon_remote_buffer drb;
	photonBuffer db;
	uint64_t cookie;
	int rc;
	uint32_t request_id;

	dbg_info("(%d, %p, %lu, %lu, %p)", proc, ptr, size, rbuf->size, request);

	if (buffertable_find_containing( (void *)ptr, (int)size, &db) != 0) {
		log_err("Tried posting a os_get_direct() into a buffer that's not registered");
		return -1;
	}
	
	/* TODO: factor out backend-dependent memory handles */
	drb.lkey = rbuf->priv.key0;
	drb.rkey = rbuf->priv.key1;

	request_id = INC_COUNTER(curr_cookie);
	dbg_info("Incrementing curr_cookie_count to: %d", request_id);

	cookie = (( (uint64_t)proc)<<32) | request_id;
	dbg_info("Posted Cookie: %u/%u/%"PRIo64, proc, request_id, cookie);

	{

		rc = __photon_backend->rdma_get(proc, (uintptr_t)ptr, rbuf->addr,
										rbuf->size, db, &drb, cookie);
		
		if (rc != PHOTON_OK) {
			dbg_err("RDMA GET failed for %lu\n", cookie);
			goto error_exit;
		}
	}

	if (request != NULL) {
		photonRequest req;

		*request = request_id;

		req = __photon_get_request();
		if (!req) {
			log_err("Couldn't allocate request\n");
			goto error_exit;
		}
		req->id = request_id;
		req->state = REQUEST_PENDING;
		// photon_post_os_get() causes an RDMA transfer, but its own completion is
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

	return PHOTON_OK;

error_exit:
	if (request != NULL) {
		*request = NULL_COOKIE;
	}
	return PHOTON_ERROR;
}

static int _photon_send_FIN(int proc) {
	photonRemoteBuffer drb;
	photonFINLedgerEntry entry;
	int curr, num_entries, rc;
	uint64_t cookie;

	dbg_info("(%d)", proc);

	if (photon_processes[proc].curr_remote_buffer->request == NULL_COOKIE) {
		log_err("Cannot send FIN, curr_remote_buffer->request is NULL_COOKIE");
		goto error_exit;
	}

	drb = photon_processes[proc].curr_remote_buffer;
	curr = photon_processes[proc].remote_FIN_ledger->curr;
	entry = &photon_processes[proc].remote_FIN_ledger->entries[curr];
	dbg_info("photon_processes[%d].remote_FIN_ledger->curr==%d",proc, curr);

	if( entry == NULL ) {
		log_err("entry is NULL for proc=%d",proc);
		return 1;
	}

	entry->header = 1;
	entry->request = drb->request;
	entry->footer = 1;

	{
		uintptr_t rmt_addr;
		rmt_addr  = photon_processes[proc].remote_FIN_ledger->remote.addr;
		rmt_addr += photon_processes[proc].remote_FIN_ledger->curr * sizeof(*entry);
		cookie = (( (uint64_t)proc)<<32) | NULL_COOKIE;

		rc = __photon_backend->rdma_put(proc, (uintptr_t)entry, rmt_addr, sizeof(*entry), shared_storage,
						&(photon_processes[proc].remote_rcv_info_ledger->remote), cookie);
		if (rc != PHOTON_OK) {
			dbg_err("RDMA PUT failed for %lu\n", cookie);
			goto error_exit;
		}
	}

	num_entries = photon_processes[proc].remote_FIN_ledger->num_entries;
	curr = photon_processes[proc].remote_FIN_ledger->curr;
	curr = (curr + 1) % num_entries;
	photon_processes[proc].remote_FIN_ledger->curr = curr;

	drb->request = NULL_COOKIE;

	return PHOTON_OK;

error_exit:
	return PHOTON_ERROR;
}

static int _photon_wait_any(int *ret_proc, uint32_t *ret_req) {
	int rc;

	dbg_info("remaining: %d", htable_count(reqtable));

	if (ret_req == NULL) {
		goto error_exit;
	}

	if (htable_count(reqtable) == 0) {
		log_err("No events on queue to wait on");
		goto error_exit;
	}

	while(1) {
		uint32_t cookie;
		int existed;
		photon_event_status event;
		
		rc = __photon_backend->get_event(&event);
		if (rc != PHOTON_OK) {
			dbg_err("Could not get event");
			goto error_exit;
		}		
		
		cookie = (uint32_t)( (event.id<<32)>>32);
		//fprintf(stderr,"gen_wait_any() poped an events with cookie:%x\n",cookie);
		if (cookie != NULL_COOKIE) {
			photonRequest req;
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
			*ret_proc = (uint32_t)(event.id>>32);
			return PHOTON_OK;
		}
	}

	return PHOTON_OK;
error_exit:
	return PHOTON_ERROR;
}

static int _photon_wait_any_ledger(int *ret_proc, uint32_t *ret_req) {
	static int i = -1; // this is static so we don't starve events in later processes
	int curr, num_entries;

	dbg_info("remaining: %d", htable_count(reqtable));

	if (ret_req == NULL || ret_proc == NULL) {
		goto error_exit;
	}

	if (htable_count(ledger_reqtable) == 0) {
		log_err("No events on queue to wait_one()");
		goto error_exit;
	}

	while(1) {
		photonFINLedgerEntry curr_entry;
		int exists;

		i=(i+1)%_photon_nproc;

		// check if an event occurred on the RDMA end of things
		curr = photon_processes[i].local_FIN_ledger->curr;
		curr_entry = &(photon_processes[i].local_FIN_ledger->entries[curr]);

		if (curr_entry->header != (uint8_t) 0 && curr_entry->footer != (uint8_t) 0) {
			void *test;
			dbg_info("Wait All In: %d/%u", photon_processes[i].local_FIN_ledger->curr, curr_entry->request);
			curr_entry->header = 0;
			curr_entry->footer = 0;

			exists = htable_remove(ledger_reqtable, (uint64_t)curr_entry->request, &test);
			if (exists != -1) {
				photonRequest req;
				req = test;
				*ret_req = curr_entry->request;
				*ret_proc = i;
				SAFE_LIST_REMOVE(req, list);
				SAFE_LIST_INSERT_HEAD(&free_reqs_list, req, list);
				break;
			}

			num_entries = photon_processes[i].local_FIN_ledger->num_entries;
			curr = photon_processes[i].local_FIN_ledger->curr;
			curr = (curr + 1) % num_entries;
			photon_processes[i].local_FIN_ledger->curr = curr;
			dbg_info("Wait All Out: %d", curr);
		}
	}

	return PHOTON_OK;

 error_exit:
	return PHOTON_ERROR;
}

static int _photon_probe_ledger(int proc, int *flag, int type, photonStatus status) {
	photonRILedger ledger;
	photonRILedgerEntry entry_iterator;
	int i, j;
	int start, end;

	dbg_info("(%d, %d)", proc, type);

	*flag = -1;

	if (proc == PHOTON_ANY_SOURCE) {
		start = 0;
		end = _photon_nproc;
	}
	else {
		start = proc;
		end = proc+1;
	}

	for (i=start; i<end; i++) {
		
		switch (type) {
		case PHOTON_SEND_LEDGER:
			ledger = photon_processes[i].local_snd_info_ledger;
			break;
		case PHOTON_RECV_LEDGER:
			ledger = photon_processes[i].local_rcv_info_ledger;
			break;
		default:
			dbg_err("unknown ledger type");
			goto error_exit;
		}
		
		for (j=0; j<ledger->num_entries; j++) {
			entry_iterator = &(ledger->entries[j]);
			if (entry_iterator->header && entry_iterator->footer && (entry_iterator->tag > 0)) {
				*flag = i;
				status->src_addr = i;
				status->request = entry_iterator->request;
				status->tag = entry_iterator->tag;
				status->size = entry_iterator->size;

				dbg_info("Request: %u", entry_iterator->request);
				dbg_info("Context: %u", entry_iterator->rkey);
				dbg_info("Address: %p", (void *)entry_iterator->addr);
				dbg_info("Size: %u", entry_iterator->size);
				dbg_info("Tag: %d", entry_iterator->tag);

				*flag = 1;

				return PHOTON_OK;
			}
		}
	}
	
 error_exit:
	return PHOTON_ERROR;
}

/* begin I/O */
static int _photon_io_init(char *file, int amode, MPI_Datatype view, int niter) {
	/* forwarders do our I/O for now */
	if (__photon_forwarder != NULL) {
		return __photon_forwarder->io_init(photon_processes, file, amode, view, niter);
	}
	else {
		return PHOTON_ERROR;
	}
}

static int _photon_io_finalize() {
	/* forwarders do our I/O for now */
	if (__photon_forwarder != NULL) {
		return __photon_forwarder->io_finalize(photon_processes);
	}
	else {
		return PHOTON_ERROR;
	}
}

static int _photon_rdma_put(int proc, uintptr_t laddr, uintptr_t raddr, uint64_t size,
							photonBuffer lbuf, photonRemoteBuffer rbuf, uint64_t id) {
	return PHOTON_OK;
}

static int _photon_rdma_get(int proc, uintptr_t laddr, uintptr_t raddr, uint64_t size,
							photonBuffer lbuf, photonRemoteBuffer rbuf, uint64_t id) {
	return PHOTON_OK;
}


static int _photon_rdma_send(int proc, uintptr_t laddr, uintptr_t raddr, uint64_t size,
							 photonBuffer lbuf, photonRemoteBuffer rbuf, uint64_t id) {
	return PHOTON_OK;
}

static int _photon_rdma_recv(int proc, uintptr_t laddr, uintptr_t raddr, uint64_t size,
							 photonBuffer lbuf, photonRemoteBuffer rbuf, uint64_t id) {
	return PHOTON_OK;
}

static int _photon_get_event(photonEventStatus stat) {

	return PHOTON_OK;
}

/* TODO */
#ifdef PHOTON_MULTITHREADED
static inline int __photon_complete_ledger_req(uint32_t cookie) {
	photonRequest tmp_req;

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

static inline int __photon_complete_evd_req(uint32_t cookie) {
	photonRequest tmp_req;

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

static void *__photon_req_watcher(void *arg) {
	int i, rc;
	int curr;
	uint32_t cookie;
	photon_event_status event;

	dbg_info("reqs watcher started");

	while(1) {
		// First we poll for CQEs and clear reqs waiting on them.
		// We don't want to spend too much time on this before moving to ledgers.
		
		//should get more events per call here 
		rc = __photon_backend->get_event(&event);
		if (rc != PHOTON_OK) {
			dbg_err("Could not get event");
			goto error_exit;
		}		
		
		cookie = (uint32_t)( (event.id<<32)>>32);

		if (cookie == NULL_COOKIE)
			continue;
		
		if (!__photon_complete_ledger_req(cookie))
			continue;
		
		if (!__photon_complete_evd_req(cookie))
			continue;
		
		// TODO: Is this the only other possibility?
		if( DEC_COUNTER(handshake_rdma_write) <= 0 )
			log_err("handshake_rdma_write_count is negative");

		for(i = 0; i < _photon_nproc; i++) {
			photon_rdma_FIN_ledger_entry_t *curr_entry;
			curr = photon_processes[i].local_FIN_ledger->curr;
			curr_entry = &(photon_processes[i].local_FIN_ledger->entries[curr]);
			if (curr_entry->header != (uint8_t) 0 && curr_entry->footer != (uint8_t) 0) {
				dbg_info("found: %d/%u", curr, curr_entry->request);
				curr_entry->header = 0;
				curr_entry->footer = 0;

				if (__photon_complete_ledger_req(curr_entry->request))
					log_err("couldn't find req for FIN ledger: %u", curr_entry->request);

				photon_processes[i].local_FIN_ledger->curr = (photon_processes[i].local_FIN_ledger->curr + 1) % photon_processes[i].local_FIN_ledger->num_entries;
				dbg_info("%d requests left in reqtable", htable_count(ledger_reqtable));
			}
		}
	}

	pthread_exit(NULL);
}

#endif

/* utility method to get backend-specific buffer info */
int _photon_get_buffer_private(void *buf, uint64_t size, photonBufferPriv ret_priv) {
	photonBuffer db;

	if (buffertable_find_exact(buf, size, &db) == 0) {
		return photon_buffer_get_private(db, ret_priv);
	}
	else {
		return PHOTON_ERROR;
	}
}

#ifdef HAVE_XSP
int photon_xsp_lookup_proc(libxspSess *sess, ProcessInfo **ret_pi, int *index) {
	int i;
	
	for (i = 0; i < _photon_nproc; i++) {
		if (photon_processes[i].sess &&
			!xsp_sesscmp(photon_processes[i].sess, sess)) {
			if (index)
				*index = i;
			*ret_pi = &photon_processes[i];
			return PHOTON_OK;
		}
	}
	
	if (index)
		*index = -1;
	*ret_pi = NULL;
	return PHOTON_ERROR;
}

int photon_xsp_unused_proc(ProcessInfo **ret_pi, int *index) {
	int i;

	/* find a process struct that has no session... */
	for (i = 0; i < _photon_nproc; i++) {
		if (!photon_processes[i].sess)
			break;
	}
	
	if (i == _photon_nproc) {
		if (index)
			*index = -1;
		*ret_pi = NULL;
		return PHOTON_ERROR;
	}
	
	if (index)
		*index = i;
	
	*ret_pi = &photon_processes[i];
	return PHOTON_OK;
}
#endif
