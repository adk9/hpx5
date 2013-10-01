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
#include "logging.h"
#include "counter.h"
#include "htable.h"

int ugni_initialized(void);
int ugni_init(photonConfig cfg);
int ugni_finalize(void);
int ugni_test(uint32_t request, int *flag, int *type, photonStatus status);
int ugni_wait(uint32_t request);
int ugni_wait_ledger(uint32_t request);
int ugni_post_recv_buffer_rdma(int proc, char *ptr, uint32_t size, int tag, uint32_t *request);
int ugni_post_send_buffer_rdma(int proc, char *ptr, uint32_t size, int tag, uint32_t *request);
int ugni_post_send_request_rdma(int proc, uint32_t size, int tag, uint32_t *request);
int ugni_wait_recv_buffer_rdma(int proc, int tag);
int ugni_wait_send_buffer_rdma(int proc, int tag);
int ugni_wait_send_request_rdma(int tag);
int ugni_post_os_put(int proc, char *ptr, uint32_t size, int tag, uint32_t remote_offset, uint32_t *request);
int ugni_post_os_get(int proc, char *ptr, uint32_t size, int tag, uint32_t remote_offset, uint32_t *request);
int ugni_send_FIN(int proc);
int ugni_wait_any(int *ret_proc, uint32_t *ret_req);
int ugni_wait_any_ledger(int *ret_proc, uint32_t *ret_req);
int ugni_probe_ledger(int proc, int *flag, int type, photonStatus status);

ugni_cnct_ctx ugni_ctx = {
	.gemini_dev = "ipogif0",
};
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


int ugni_initialized() {
	if (__initialized)
		return PHOTON_OK;
	else
		return PHOTON_ERROR_NOINIT;
}

int ugni_init(photonConfig cfg) {
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

	if (photon_buffer_register(shared_storage) != 0) {
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

int ugni_finalize(void) {

	return PHOTON_OK;
}

int ugni_test(uint32_t request, int *flag, int *type, photonStatus status) {

	return PHOTON_OK;
}

int ugni_wait(uint32_t request) {

	return PHOTON_OK;
}

int ugni_wait_ledger(uint32_t request) {

	return PHOTON_OK;
}

int ugni_post_recv_buffer_rdma(int proc, char *ptr, uint32_t size, int tag, uint32_t *request) {

	return PHOTON_OK;
}

int ugni_post_send_buffer_rdma(int proc, char *ptr, uint32_t size, int tag, uint32_t *request) {


	return PHOTON_OK;
}

int ugni_post_send_request_rdma(int proc, uint32_t size, int tag, uint32_t *request) {

	return PHOTON_OK;
}

int ugni_wait_recv_buffer_rdma(int proc, int tag) {

	return PHOTON_OK;
}

int ugni_wait_send_buffer_rdma(int proc, int tag) {

	return PHOTON_OK;
}

int ugni_wait_send_request_rdma(int tag) {

	return PHOTON_OK;
}

int ugni_post_os_put(int proc, char *ptr, uint32_t size, int tag, uint32_t remote_offset, uint32_t *request) {

	return PHOTON_OK;
}

int ugni_post_os_get(int proc, char *ptr, uint32_t size, int tag, uint32_t remote_offset, uint32_t *request) {

	return PHOTON_OK;
}

int ugni_send_FIN(int proc) {

	return PHOTON_OK;
}

int ugni_wait_any(int *ret_proc, uint32_t *ret_req) {

	return PHOTON_OK;
}

int ugni_wait_any_ledger(int *ret_proc, uint32_t *ret_req) {

	return PHOTON_OK;
}

int ugni_probe_ledger(int proc, int *flag, int type, photonStatus status) {

	return PHOTON_OK;
}
