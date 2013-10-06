#define _GNU_SOURCE
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#include "photon_buffer.h"
#include "photon_buffertable.h"
#include "photon_ugni_connect.h"
#include "photon_ugni_exchange.h"

#include "photon_ugni.h"
#include "counter.h"
#include "htable.h"
#include "utility_functions.h"

static int ugni_initialized(void);
static int ugni_init(photonConfig cfg);
static int ugni_finalize(void);
static int ugni_test(uint32_t request, int *flag, int *type, photonStatus status);
static int ugni_wait(uint32_t request);
static int ugni_wait_ledger(uint32_t request);
static int ugni_post_recv_buffer_rdma(int proc, char *ptr, uint32_t size, int tag, uint32_t *request);
static int ugni_post_send_buffer_rdma(int proc, char *ptr, uint32_t size, int tag, uint32_t *request);
static int ugni_post_send_request_rdma(int proc, uint32_t size, int tag, uint32_t *request);
static int ugni_wait_recv_buffer_rdma(int proc, int tag);
static int ugni_wait_send_buffer_rdma(int proc, int tag);
static int ugni_wait_send_request_rdma(int tag);
static int ugni_post_os_put(int proc, char *ptr, uint32_t size, int tag, uint32_t remote_offset, uint32_t *request);
static int ugni_post_os_get(int proc, char *ptr, uint32_t size, int tag, uint32_t remote_offset, uint32_t *request);
static int ugni_send_FIN(int proc);
static int ugni_wait_any(int *ret_proc, uint32_t *ret_req);
static int ugni_wait_any_ledger(int *ret_proc, uint32_t *ret_req);
static int ugni_probe_ledger(int proc, int *flag, int type, photonStatus status);

static ugni_cnct_ctx ugni_ctx;
photonBuffer shared_storage;

extern int _photon_nproc;
extern int _photon_myrank;
extern int _photon_forwarder;

static UgniProcessInfo *ugni_processes;
static htable_t *reqtable, *ledger_reqtable;
static photonRequest requests;
static int num_requests;

static LIST_HEAD(freereqs, photon_req_t) free_reqs_list;
static LIST_HEAD(unreapedevdreqs, photon_req_t) unreaped_evd_reqs_list;
static LIST_HEAD(unreapedledgerreqs, photon_req_t) unreaped_ledger_reqs_list;
static LIST_HEAD(pendingreqs, photon_req_t) pending_reqs_list;

static int __initialized = 0;
DEFINE_COUNTER(curr_cookie, uint32_t)
DEFINE_COUNTER(handshake_rdma_write, uint32_t)

/* we are now a Photon backend */
struct photon_backend_t photon_ugni_backend = {
	.context = &ugni_ctx,
	.initialized = ugni_initialized,
	.init = ugni_init,
	.finalize = ugni_finalize,
	.test = ugni_test,
	.wait = ugni_wait,
	.wait_ledger = ugni_wait,
	.post_recv_buffer_rdma = ugni_post_recv_buffer_rdma,
	.post_send_buffer_rdma = ugni_post_send_buffer_rdma,
	.wait_recv_buffer_rdma = ugni_wait_recv_buffer_rdma,
	.wait_send_buffer_rdma = ugni_wait_send_buffer_rdma,
	.wait_send_request_rdma = ugni_wait_send_request_rdma,
	.post_os_put = ugni_post_os_put,
	.post_os_get = ugni_post_os_get,
	.send_FIN = ugni_send_FIN,
	.probe_ledger = ugni_probe_ledger,
#ifndef PHOTON_MULTITHREADED
	.wait_any = ugni_wait_any,
	.wait_any_ledger = ugni_wait_any_ledger
#endif
};

static inline photonRequest __ugni_get_request() {
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

///////////////////////////////////////////////////////////////////////////////
// __ugni_nbpop_event() is non blocking and returns:
// -1 if an error occured.
//	0 if the request (req) specified in the argument has completed.
//	1 if either no event was in the queue, or there was an event but not for the specified request (req).
static int __ugni_nbpop_event(photonRequest req) {
	int status;
	uint32_t cookie;
	gni_post_descriptor_t *event_post_desc_ptr;
	gni_cq_entry_t current_event;	

	dbg_info("(%d)", req->id);

	if (req->state == REQUEST_PENDING) {
		status = get_cq_event(ugni_ctx.local_cq_handle, 1, 0, &current_event);
		if (status == 0) {
			status = GNI_GetCompleted(ugni_ctx.local_cq_handle, current_event, &event_post_desc_ptr);
			if (!event_post_desc_ptr) {
				dbg_err("GNI_GetCompleted returned NULL, ERROR status: %s (%d)", gni_err_str[status], status);
				goto error_exit;
			}
		}
		else if (status == 3) {
			/* no event was found */
			return 1;
		}
		else {
			/* rc == 2 is an overrun */
			dbg_err("Error getting CQ event, possible overrun: %d\n", status);
			goto error_exit;
		}

		cookie = (uint32_t) ((event_post_desc_ptr->post_id<<32)>>32);
		dbg_info("Got post_id: %"PRIx64, event_post_desc_ptr->post_id);
		dbg_info("To RID: %x", cookie);
		
		if (cookie == req->id) {
			req->state = REQUEST_COMPLETED;

			dbg_info("removing event with cookie: %u", cookie);
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
//		call to __ugni_nbpop_ledger() poped the FIN that corresponds to "req".
//	1 if the request is pending and the FIN has not arrived yet
static int __ugni_nbpop_ledger(photonRequest req) {
	void *test;
	int curr, i=-1;

	dbg_info("(%d)",req->id);

	if(req->state == REQUEST_PENDING) {

		// Check if an entry of the FIN LEDGER was written with "id" equal to to "req"
		for(i = 0; i < _photon_nproc; i++) {
			photonFINLedgerEntry curr_entry;
			curr = ugni_processes[i].local_FIN_ledger->curr;
			curr_entry = &(ugni_processes[i].local_FIN_ledger->entries[curr]);
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
					int num = ugni_processes[i].local_FIN_ledger->num_entries;
					int new_curr = (ugni_processes[i].local_FIN_ledger->curr + 1) % num;
					ugni_processes[i].local_FIN_ledger->curr = new_curr;
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

				int num = ugni_processes[i].local_FIN_ledger->num_entries;
				int new_curr = (ugni_processes[i].local_FIN_ledger->curr + 1) % num;
				ugni_processes[i].local_FIN_ledger->curr = new_curr;
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

/* this doesn't actually get called in the current code */
static int __ugni_wait_one() {
	uint64_t cookie;
	photonRequest tmp_req;	
	void *test;
	int status;

	gni_post_descriptor_t *event_post_desc_ptr;
	gni_cq_entry_t current_event;	

	dbg_info("remaining: %d+%d", htable_count(reqtable), GET_COUNTER(handshake_rdma_write));

	if ( (htable_count(reqtable) == 0) && (GET_COUNTER(handshake_rdma_write) == 0) ) {
		dbg_info("No events on queue, or handshake writes pending to wait on");
		goto error_exit;
	}

	status = get_cq_event(ugni_ctx.local_cq_handle, 1, 0, &current_event);
	if (status == 0) {
		status = GNI_GetCompleted(ugni_ctx.local_cq_handle, current_event, &event_post_desc_ptr);
		if (status != GNI_RC_SUCCESS) {
			dbg_err("GNI_GetCompleted  data ERROR status: %s (%d)", gni_err_str[status], status);
		}
	}
	else {
		/* rc == 2 is an overrun */
		dbg_err("Error getting CQ event: %d\n", status);
	}

	cookie = event_post_desc_ptr->post_id;
	dbg_info("received event with cookie:%"PRIx64, cookie);

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

	return PHOTON_OK;
error_exit:
	return PHOTON_ERROR;
}

int ugni_initialized() {
	if (__initialized)
		return PHOTON_OK;
	else
		return PHOTON_ERROR_NOINIT;
}

static int ugni_init(photonConfig cfg) {
	int i;
	char *buf;
	int bufsize, offset;
	int info_ledger_size, FIN_ledger_size;
	
	if (__initialized != 0) {
		log_err("Error: already initialized/initializing");
		goto error_exit;
	}

	_photon_nproc = cfg->nproc;
	_photon_myrank = (int)cfg->address;
	_photon_forwarder = cfg->use_forwarder;

	dbg_info("(nproc %d, rank %d)", _photon_nproc, _photon_myrank);

	if (cfg->eth_dev) {
		ugni_ctx.gemini_dev = cfg->eth_dev;
	}
	else {
		ugni_ctx.gemini_dev = "ipogif0";
	}
	
	// __initialized: 0 - not; -1 - initializing; 1 - initialized
	__initialized = -1;
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

	ugni_processes = (UgniProcessInfo *) malloc(sizeof(UgniProcessInfo) * (_photon_nproc));
	if (!ugni_processes) {
		log_err("Couldn't allocate process information");
		goto error_exit_lrt;
	}

	// Set it to zero, so that we know if it ever got initialized
	memset(ugni_processes, 0, sizeof(UgniProcessInfo) * (_photon_nproc));

	for(i = 0; i < _photon_nproc; i++) {
		ugni_processes[i].curr_remote_buffer = photon_remote_buffer_create();
		if(!ugni_processes[i].curr_remote_buffer) {
			log_err("Couldn't allocate process remote buffer information");
			goto error_exit_gp;
		}
	}

	dbg_info("alloc'd process info");

	/* setup the GEMINI device context */
	if(__ugni_init_context(&ugni_ctx)) {
		log_err("Could not initialize UGNI GEMINI context");
		goto error_exit_crb;
	}

	/* establish the endpoints */
	if(__ugni_connect_peers(&ugni_ctx)) {
		log_err("Could not connect peers");
		goto error_exit_crb;
	}

	// Everything is x2 cause we need a local and a remote copy of each ledger.
	// Remote Info (_ri_) ledger has an additional x2 cause we need "send-info" and "receive-info" ledgers.
	info_ledger_size = 2 * 2 * sizeof(struct photon_ri_ledger_entry_t) * LEDGER_SIZE * (_photon_nproc);
	FIN_ledger_size  = 2 * sizeof(struct photon_rdma_FIN_ledger_entry_t) * LEDGER_SIZE * (_photon_nproc);
	bufsize = info_ledger_size + FIN_ledger_size;
	buf = malloc(bufsize);
	if (!buf) {
		log_err("Couldn't allocate ledgers");
		goto error_exit_ugni_cnct;
	}
	dbg_info("Bufsize: %d", bufsize);
	
	shared_storage = photon_buffer_create(buf, bufsize);
	if (!shared_storage) {
		log_err("Couldn't register shared storage");
		goto error_exit_buf;
	}

	if (photon_buffer_register(shared_storage, &ugni_ctx) != 0) {
		log_err("couldn't register local buffer for the ledger entries");
		goto error_exit_ss;
	}

	if (__ugni_setup_ri_ledgers(ugni_processes, buf, LEDGER_SIZE) != 0) {
		log_err("couldn't setup snd/rcv info ledgers");
		goto error_exit_listeners;
	}

	// skip 4 ledgers (rcv info local, rcv info remote, snd info local, snd info remote)
	offset = 4 * sizeof(struct photon_ri_ledger_entry_t) * LEDGER_SIZE * (_photon_nproc);
	if (__ugni_setup_FIN_ledger(ugni_processes, buf + offset, LEDGER_SIZE) != 0) {
		log_err("couldn't setup send ledgers");
		goto error_exit_ri_ledger;
	}
	
	if (__ugni_exchange_ri_ledgers(ugni_processes) != 0) {
		log_err("couldn't exchange rdma ledgers");
		goto error_exit_listeners;
	}

	if (__ugni_exchange_FIN_ledger(ugni_processes) != 0) {
		log_err("couldn't exchange send ledgers");
		goto error_exit_FIN_ledger;
	}

	__initialized = 1;
	
	dbg_info("ended successfully =============");

	return PHOTON_OK;

 error_exit_FIN_ledger:
 error_exit_ri_ledger:
 error_exit_listeners:
 error_exit_ss:
	photon_buffer_free(shared_storage);
 error_exit_buf:
	if (buf)
		free(buf);
 error_exit_ugni_cnct:
 error_exit_crb:
	for(i = 0; i < _photon_nproc; i++) {
		if (ugni_processes[i].curr_remote_buffer != NULL) {
			photon_remote_buffer_free(ugni_processes[i].curr_remote_buffer);
		}
	}
 error_exit_gp:
	free(ugni_processes);
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
	return PHOTON_ERROR;
}

static int ugni_finalize(void) {
	
	return PHOTON_OK;
}

static int ugni_test(uint32_t request, int *flag, int *type, photonStatus status) {
	photonRequest req;
	void *test;
	int ret_val;

	dbg_info("(%d)", request);

	if( __initialized <= 0 ) {
		log_err("Library not initialized.	 Call photon_init() first");
		dbg_info("returning -1");
		return -1;
	}

	if (htable_lookup(reqtable, (uint64_t)request, &test) != 0) {
		if (htable_lookup(ledger_reqtable, (uint64_t)request, &test) != 0) {
			dbg_info("Request is not in either request-table");
			// Unlike ugni_wait(), we might call ugni_test() multiple times on a request,
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
		ret_val = __ugni_nbpop_ledger(req);
	}
	else {
		if( type != NULL ) *type = 0;
		ret_val = __ugni_nbpop_event(req);
	}
#endif

	if( !ret_val ) {
		*flag = 1;
		status->src_addr = req->proc;
		status->tag = req->tag;
		status->count = 1;
		status->error = 0;
		dbg_info("returning 0, flag:1");
		return PHOTON_OK;
	}
	else if( ret_val > 0 ) {
		dbg_info("returning 0, flag:0");
		*flag = 0;
		return PHOTON_OK;
	}
	else {
		dbg_info("returning -1, flag:0");
		*flag = 0;
		return -1;
	}
}

static int ugni_wait(uint32_t request) {

	return PHOTON_OK;
}

static int ugni_wait_ledger(uint32_t request) {

	return PHOTON_OK;
}

static int ugni_post_recv_buffer_rdma(int proc, char *ptr, uint32_t size, int tag, uint32_t *request) {
	photonBuffer db;
	uint64_t cookie;
	photonRILedgerEntry entry;
	int curr, num_entries, err;
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
		proc = ugni_wait_send_request_rdma(tag);
	}

	curr = ugni_processes[proc].remote_rcv_info_ledger->curr;
	entry = &ugni_processes[proc].remote_rcv_info_ledger->entries[curr];

	// fill in what we're going to transfer
	entry->header = 1;
	entry->request = request_id;
	entry->rkey = 0;
	entry->addr = (uintptr_t) ptr;
	entry->size = size;
	entry->tag = tag;
	entry->footer = 1;
	entry->qword1 = db->mdh.qword1;
	entry->qword2 = db->mdh.qword2;

	dbg_info("Post recv");
	dbg_info("Request: %u", entry->request);
	dbg_info("rkey: %u", entry->rkey);
	dbg_info("Address: %p", (void *)entry->addr);
	dbg_info("Size: %u", entry->size);
	dbg_info("Tag: %d", entry->tag);
	dbg_info("MDH: 0x%016lx / 0x%016lx", entry->qword1, entry->qword2);

	{
		gni_post_descriptor_t fma_desc;
		uintptr_t rmt_addr;

        rmt_addr  = ugni_processes[proc].remote_rcv_info_ledger->remote.addr;
		rmt_addr += ugni_processes[proc].remote_rcv_info_ledger->curr * sizeof(*entry);
		cookie = (( (uint64_t)proc<<32) | NULL_COOKIE);

		dbg_info("Posting cookie: %"PRIx64, cookie);

		fma_desc.type = GNI_POST_FMA_PUT;
		fma_desc.cq_mode = GNI_CQMODE_GLOBAL_EVENT;
		fma_desc.dlvr_mode = GNI_DLVMODE_PERFORMANCE;
        fma_desc.local_addr = (uint64_t) entry;
        fma_desc.local_mem_hndl = shared_storage->mdh;
		fma_desc.remote_addr = (uint64_t) rmt_addr;
        fma_desc.remote_mem_hndl = ugni_processes[proc].remote_rcv_info_ledger->remote.mdh;
        fma_desc.length = sizeof(*entry);
        fma_desc.post_id = cookie;
		fma_desc.rdma_mode = GNI_RDMAMODE_FENCE;
        fma_desc.src_cq_hndl = ugni_ctx.local_cq_handle;

		err = GNI_PostFma(ugni_ctx.ep_handles[proc], &fma_desc);
		if (err != GNI_RC_SUCCESS) {
			log_err("GNI_PostFma data ERROR status: %s (%d)\n", gni_err_str[err], err);
			goto error_exit;
		}
		/* the associated CQ events will eventually get popped by __ugni_nbpop_event() when app waits
		   we could wait for it here but that means handling any other events that might pop up
		   another option is to create separate CQs for these FMA ops */
		
		/*
		gni_post_descriptor_t *event_post_desc_ptr;
		gni_cq_entry_t current_event;
		err = get_cq_event(ugni_ctx.local_cq_handle, 1, 0, &current_event);
		if (err == 0) {
			err = GNI_GetCompleted(ugni_ctx.local_cq_handle, current_event, &event_post_desc_ptr);
			if (err != GNI_RC_SUCCESS) {
				if (event_post_desc_ptr == NULL) {
					dbg_err("GNI_GetCompleted returned NULL, ERROR status: %s (%d)", gni_err_str[err], err);
					goto error_exit;
				}
			}
			dbg_info("Got post_id: %"PRIx64, event_post_desc_ptr->post_id);
		}
		*/

		dbg_info("GNI_PostFma data transfer successful: %"PRIx64, cookie);
	}

	// TODO: I don't think that this is a sufficient solution.
	// Keep track of the dat_ep_post_rdma_write()s that do not insert an entry into "reqtable", so
	// that __ugni_wait_one() is able to wait on this event's completion
	// this is actually kind of pointless...
	INC_COUNTER(handshake_rdma_write);

	if (request != NULL) {
		photonRequest req;

		req = __ugni_get_request();
		if (!req) {
			log_err("Couldn't allocate request\n");
			goto error_exit;
		}
		req->id = request_id;
		req->state = REQUEST_PENDING;
		// ugni_post_recv_buffer_rdma() initiates a receiver initiated handshake.	For this reason,
		// we don't care when the function is completed, but rather when the transfer associated with
		// this handshake is completed.	 This will be reflected in the LEDGER by the corresponding
		// ugni_send_FIN() posted by the sender.
		req->type = LEDGER;
		req->proc = proc;
		req->tag = tag;
		/* there is no point to this either...i think we can remove it */
		SAFE_LIST_INSERT_HEAD(&pending_reqs_list, req, list);

		dbg_info("Inserting the RDMA request into the request table: %d/%p", request_id, req);

		if (htable_insert(ledger_reqtable, (uint64_t)request_id, req) != 0) {
			// this is bad, we've submitted the request, but we can't track it
			log_err("Couldn't save request in hashtable");
		}
		*request = request_id;
	}

	num_entries = ugni_processes[proc].remote_rcv_info_ledger->num_entries;
	curr = ugni_processes[proc].remote_rcv_info_ledger->curr;
	curr = (curr + 1) % num_entries;
	ugni_processes[proc].remote_rcv_info_ledger->curr = curr;

	dbg_info("New curr (proc=%d): %u", proc, ugni_processes[proc].remote_rcv_info_ledger->curr);

	return PHOTON_OK;

error_exit:
	if (request != NULL) {
		*request = NULL_COOKIE;
	}
	return PHOTON_ERROR;
}

static int ugni_post_send_buffer_rdma(int proc, char *ptr, uint32_t size, int tag, uint32_t *request) {


	return PHOTON_OK;
}

static int ugni_post_send_request_rdma(int proc, uint32_t size, int tag, uint32_t *request) {

	return PHOTON_OK;
}

static int ugni_wait_recv_buffer_rdma(int proc, int tag) {
	photonRemoteBuffer curr_remote_buffer;
	photonRILedgerEntry curr_entry, entry_iterator;
	struct photon_ri_ledger_entry_t tmp_entry;
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
	curr_remote_buffer = ugni_processes[proc].curr_remote_buffer;
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
	dbg_info("curr == %d", ugni_processes[proc].local_rcv_info_ledger->curr);

	curr = ugni_processes[proc].local_rcv_info_ledger->curr;
	curr_entry = &(ugni_processes[proc].local_rcv_info_ledger->entries[curr]);

	dbg_info("looking in position %d/%p", ugni_processes[proc].local_rcv_info_ledger->curr, curr_entry);

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
			curr = ugni_processes[proc].local_rcv_info_ledger->curr;
			num_entries = ugni_processes[proc].local_rcv_info_ledger->num_entries;
			curr = (curr + count++) % num_entries;
			entry_iterator = &(ugni_processes[proc].local_rcv_info_ledger->entries[curr]);
		}
	}
	while(still_searching);

	// If it wasn't the first pending receive request, swap the one we will serve ( entry_iterator) with
	// the first pending (curr_entry) in the info ledger, so that we can increment the current pointer
	// (ugni_processes[proc].local_rcv_info_ledger->curr) and skip the request we will serve without losing any
	// pending requests.
	if( entry_iterator != curr_entry ) {
		tmp_entry = *entry_iterator;
		*entry_iterator = *curr_entry;
		*curr_entry = tmp_entry;
	}

	ugni_processes[proc].curr_remote_buffer->request = curr_entry->request;
	ugni_processes[proc].curr_remote_buffer->rkey = curr_entry->rkey;
	ugni_processes[proc].curr_remote_buffer->addr = curr_entry->addr;
	ugni_processes[proc].curr_remote_buffer->size = curr_entry->size;
	ugni_processes[proc].curr_remote_buffer->tag  = curr_entry->tag;
	ugni_processes[proc].curr_remote_buffer->mdh.qword1  = curr_entry->qword1;
	ugni_processes[proc].curr_remote_buffer->mdh.qword2  = curr_entry->qword2;

	dbg_info("Request: %u", curr_entry->request);
	dbg_info("rkey: %u", curr_entry->rkey);
	dbg_info("Addr: %p", (void *)curr_entry->addr);
	dbg_info("Size: %u", curr_entry->size);
	dbg_info("Tag: %d",	curr_entry->tag);
	dbg_info("MDH: 0x%016lx / 0x%016lx", curr_entry->qword1, curr_entry->qword2);

	curr_entry->header = 0;
	curr_entry->footer = 0;

	num_entries = ugni_processes[proc].local_rcv_info_ledger->num_entries;
	curr = ugni_processes[proc].local_rcv_info_ledger->curr;
	curr = (curr + 1) % num_entries;
	ugni_processes[proc].local_rcv_info_ledger->curr = curr;

	dbg_info("new curr == %d", ugni_processes[proc].local_rcv_info_ledger->curr);

normal_exit:
	return PHOTON_OK;
error_exit:
	return PHOTON_ERROR;
}

static int ugni_wait_send_buffer_rdma(int proc, int tag) {

	return PHOTON_OK;
}

static int ugni_wait_send_request_rdma(int tag) {

	return PHOTON_OK;
}

static int ugni_post_os_put(int proc, char *ptr, uint32_t size, int tag, uint32_t remote_offset, uint32_t *request) {
	photonRemoteBuffer drb;
	photonBuffer db;
	uint64_t cookie;
	int err;
	uint32_t request_id;

	dbg_info("(%d, %p, %u, %u, %p)", proc, ptr, size, remote_offset, request);

	drb = ugni_processes[proc].curr_remote_buffer;

	if (drb->request == NULL_COOKIE) {
		log_err("Tried posting a send with no recv buffer. Have you called ugni_wait_recv_buffer_rdma() first?");
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

	cookie = (( (uint64_t)proc<<32) | request_id);

	dbg_info("Posted Cookie: %u/%u/%"PRIx64, proc, request_id, cookie);
	dbg_info("Posting to remote buffer %p", (void *)drb->addr);

	{
		gni_post_descriptor_t rdma_desc;
		
		rdma_desc.type = GNI_POST_FMA_PUT;
		rdma_desc.cq_mode = GNI_CQMODE_GLOBAL_EVENT;
		rdma_desc.dlvr_mode = GNI_DLVMODE_PERFORMANCE;
        rdma_desc.local_addr = (uint64_t) ptr;
        rdma_desc.local_mem_hndl = db->mdh;
		rdma_desc.remote_addr = (uint64_t) drb->addr + (uint64_t) remote_offset;
        rdma_desc.remote_mem_hndl = drb->mdh;
        rdma_desc.length = size;
        rdma_desc.post_id = cookie;
		rdma_desc.rdma_mode = GNI_RDMAMODE_FENCE;
        rdma_desc.src_cq_hndl = ugni_ctx.local_cq_handle;
		
		err = GNI_PostFma(ugni_ctx.ep_handles[proc], &rdma_desc);
		if (err != GNI_RC_SUCCESS) {
			log_err("GNI_PostRdma data ERROR status: %s (%d)\n", gni_err_str[err], err);
			/* do some retries first */
			goto error_exit;
		}
		/* the associated CQ events will eventually get popped by __ugni_nbpop_event() when app waits
		   we could wait for it here but that means handling any other events that might pop up
		   another option is to create separate CQs for these FMA ops */
		
		dbg_info("GNI_PostRdma data transfer successful: %"PRIx64, cookie);
	}

	if (request != NULL) {
		photonRequest req;

		*request = request_id;

		req = __ugni_get_request();
		if (!req) {
			log_err("Couldn't allocate request\n");
			goto error_exit;
		}
		req->id = request_id;
		req->state = REQUEST_PENDING;
		// ugni_post_os_put() causes an RDMA transfer, but its own completion is
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

	return PHOTON_OK;

error_exit:
	if (request != NULL) {
		*request = NULL_COOKIE;
	}

	return PHOTON_ERROR;
}

int ugni_post_os_get(int proc, char *ptr, uint32_t size, int tag, uint32_t remote_offset, uint32_t *request) {

	return PHOTON_OK;
}

static int ugni_send_FIN(int proc) {
	photonRemoteBuffer drb;
	photonFINLedgerEntry entry;
	uint64_t cookie;
	int curr, num_entries, err;

	dbg_info("(%d)", proc);

	if (ugni_processes[proc].curr_remote_buffer->request == NULL_COOKIE) {
		log_err("Cannot send FIN, curr_remote_buffer->request is NULL_COOKIE");
		goto error_exit;
	}

	drb = ugni_processes[proc].curr_remote_buffer;
	curr = ugni_processes[proc].remote_FIN_ledger->curr;
	entry = &ugni_processes[proc].remote_FIN_ledger->entries[curr];
	dbg_info("ugni_processes[%d].remote_FIN_ledger->curr==%d",proc, curr);

	if( entry == NULL ) {
		log_err("entry is NULL for proc=%d",proc);
		return 1;
	}

	entry->header = 1;
	entry->request = drb->request;
	entry->footer = 1;

	{
		gni_post_descriptor_t fma_desc;
		uintptr_t rmt_addr;

		rmt_addr  = ugni_processes[proc].remote_FIN_ledger->remote.addr;
		rmt_addr += ugni_processes[proc].remote_FIN_ledger->curr * sizeof(*entry);
		/* apparently UGNI post_id should be > 1 */
		cookie = (( (uint64_t)proc<<32) | NULL_COOKIE);

		fma_desc.type = GNI_POST_FMA_PUT;
		fma_desc.cq_mode = GNI_CQMODE_GLOBAL_EVENT;
		fma_desc.dlvr_mode = GNI_DLVMODE_PERFORMANCE;
        fma_desc.local_addr = (uint64_t) entry;
        fma_desc.local_mem_hndl = shared_storage->mdh;
		fma_desc.remote_addr = (uint64_t) rmt_addr;
        fma_desc.remote_mem_hndl = ugni_processes[proc].remote_FIN_ledger->remote.mdh;
        fma_desc.length = sizeof(*entry);
        fma_desc.post_id = cookie;
		fma_desc.rdma_mode = GNI_RDMAMODE_FENCE;
        fma_desc.src_cq_hndl = ugni_ctx.local_cq_handle;

		err = GNI_PostFma(ugni_ctx.ep_handles[proc], &fma_desc);
		if (err != GNI_RC_SUCCESS) {
			log_err("GNI_PostFma data ERROR status: %s (%d)\n", gni_err_str[err], err);
			goto error_exit;
		}
		/* the associated CQ events will eventually get popped by __ugni_nbpop_event() when app waits
		   we could wait for it here but that means handling any other events that might pop up
		   another option is to create separate CQs for these FMA ops */

		dbg_info("GNI_PostFma data transfer successful: %"PRIx64, cookie);
	}

	num_entries = ugni_processes[proc].remote_FIN_ledger->num_entries;
	curr = ugni_processes[proc].remote_FIN_ledger->curr;
	curr = (curr + 1) % num_entries;
	ugni_processes[proc].remote_FIN_ledger->curr = curr;

	drb->request = NULL_COOKIE;

	return PHOTON_OK;
error_exit:
	return PHOTON_ERROR;
}

static int ugni_wait_any(int *ret_proc, uint32_t *ret_req) {

	return PHOTON_OK;
}

static int ugni_wait_any_ledger(int *ret_proc, uint32_t *ret_req) {
	
	return PHOTON_OK;
}

static int ugni_probe_ledger(int proc, int *flag, int type, photonStatus status) {

	return PHOTON_OK;
}
