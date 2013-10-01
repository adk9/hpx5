#include <strings.h>
#include "libphoton.h"
#include "photon_buffer.h"
#include "photon_buffertable.h"
#include "logging.h"

#ifdef HAVE_VERBS
#include "verbs.h"
#endif

#ifdef HAVE_UGNI
#include "photon_ugni.h"
#endif

/* Globals */
int _photon_myrank;
int _photon_nproc;
int _photon_forwarder;

static SLIST_HEAD(pendingmemregs, photon_mem_register_req) pending_mem_register_list;

#ifdef DEBUG
int _photon_start_debugging=1;
#endif

#if defined(DEBUG) || defined(CALLTRACE)
FILE *_phot_ofp;
#endif
/* END Globals */


int photon_init(photonConfig cfg) {

	if (cfg->backend != NULL) {
		if (!strncasecmp(cfg->backend, "verbs", 10)) {
#ifdef HAVE_VERBS
			__photon_backend = &photon_verbs_backend;
			photon_buffer_init(&verbs_buffer_interface);
#else
			goto error_exit;
#endif
		}
		else if (!strncasecmp(cfg->backend, "ugni", 10)) {
#ifdef HAVE_UGNI
			__photon_backend = &photon_ugni_backend;
			photon_buffer_init(&ugni_buffer_interface);
#else
			goto error_exit;
#endif
		}
		else {
			goto error_exit;
		}
	}
	else {
		log_warn("photon_init(): backend not specified, using default test backend!");
		__photon_backend = &photon_default_backend;
	}

	__photon_config = cfg;

	/* register any buffers that were requested before init */
	while( !SLIST_EMPTY(&pending_mem_register_list) ) {
		struct photon_mem_register_req *mem_reg_req;
		dbg_info("registering buffer in queue");
		mem_reg_req = SLIST_FIRST(&pending_mem_register_list);
		SLIST_REMOVE_HEAD(&pending_mem_register_list, list);
		photon_register_buffer(mem_reg_req->buffer, mem_reg_req->buffer_size);
	}

	return __photon_backend->init(cfg);

error_exit:
	log_err("photon_init(): %s support not present", cfg->backend);
	return PHOTON_ERROR;
}

int photon_finalize() {
	if(__photon_backend->initialized() != PHOTON_OK) {
		init_err();
		return PHOTON_ERROR_NOINIT;
	}

	return __photon_backend->finalize();
}

int photon_register_buffer(char *buffer, int buffer_size) {
	static int first_time = 1;
	photonBuffer db;

	dbg_info("(%p, %d)",buffer, buffer_size);

	if(__photon_backend->initialized() != PHOTON_OK) {
		struct photon_mem_register_req *mem_reg_req;
		if( first_time ) {
			SLIST_INIT(&pending_mem_register_list);
			first_time = 0;
		}
		mem_reg_req = malloc( sizeof(struct photon_mem_register_req) );
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

	db = photon_buffer_create(buffer, buffer_size);
	if (!db) {
		log_err("Couldn't register shared storage");
		goto error_exit;
	}

	dbg_info("created buffer: %p", db);

	if (photon_buffer_register(db) != 0) {
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

int photon_unregister_buffer(char *buffer, int size) {
	photonBuffer db;

	dbg_info();

	if(__photon_backend->initialized() != PHOTON_OK) {
		init_err();
		return PHOTON_ERROR_NOINIT;
	}

	if (buffertable_find_exact((void *)buffer, size, &db) != 0) {
		dbg_info("no such buffer is registered");
		goto error_exit;
	}

	if (--(db->ref_count) == 0) {
		if (photon_buffer_unregister(db) != 0) {
			goto error_exit;
		}
		buffertable_remove( db );
		photon_buffer_free(db);
	}

	return PHOTON_OK;

error_exit:
	return PHOTON_ERROR;
}

int photon_test(uint32_t request, int *flag, int *type, photonStatus status) {
	if(__photon_backend->initialized() != PHOTON_OK) {
		init_err();
		return PHOTON_ERROR_NOINIT;
	}

	return __photon_backend->test(request, flag, type, status);
}

int photon_wait(uint32_t request) {
	if(__photon_backend->initialized() != PHOTON_OK) {
		init_err();
		return PHOTON_ERROR_NOINIT;
	}

	return __photon_backend->wait(request);
}

int photon_wait_ledger(uint32_t request) {
	if(__photon_backend->initialized() != PHOTON_OK) {
		init_err();
		return PHOTON_ERROR_NOINIT;
	}

	return __photon_backend->wait_ledger(request);
}

int photon_post_recv_buffer_rdma(int proc, char *ptr, uint32_t size, int tag, uint32_t *request) {
	if(__photon_backend->initialized() != PHOTON_OK) {
		init_err();
		return PHOTON_ERROR_NOINIT;
	}

	return __photon_backend->post_recv_buffer_rdma(proc, ptr, size, tag, request);
}

int photon_post_send_buffer_rdma(int proc, char *ptr, uint32_t size, int tag, uint32_t *request) {
	if(__photon_backend->initialized() != PHOTON_OK) {
		init_err();
		return PHOTON_ERROR_NOINIT;
	}

	return __photon_backend->post_send_buffer_rdma(proc, ptr, size, tag, request);
}

int photon_post_send_request_rdma(int proc, uint32_t size, int tag, uint32_t *request) {
	if(__photon_backend->initialized() != PHOTON_OK) {
		init_err();
		return PHOTON_ERROR_NOINIT;
	}

	return __photon_backend->post_send_request_rdma(proc, size, tag, request);
}

int photon_wait_recv_buffer_rdma(int proc, int tag) {
	if(__photon_backend->initialized() != PHOTON_OK) {
		init_err();
		return PHOTON_ERROR_NOINIT;
	}

	return __photon_backend->wait_recv_buffer_rdma(proc, tag);
}

int photon_wait_send_buffer_rdma(int proc, int tag) {
	if(__photon_backend->initialized() != PHOTON_OK) {
		init_err();
		return PHOTON_ERROR_NOINIT;
	}

	return __photon_backend->wait_send_buffer_rdma(proc, tag);
}

int photon_wait_send_request_rdma(int tag) {
	if(__photon_backend->initialized() != PHOTON_OK) {
		init_err();
		return PHOTON_ERROR_NOINIT;
	}

	return __photon_backend->wait_send_request_rdma(tag);
}

int photon_post_os_put(int proc, char *ptr, uint32_t size, int tag, uint32_t remote_offset, uint32_t *request) {
	if(__photon_backend->initialized() != PHOTON_OK) {
		init_err();
		return PHOTON_ERROR_NOINIT;
	}

	return __photon_backend->post_os_put(proc, ptr, size, tag, remote_offset, request);
}

int photon_post_os_get(int proc, char *ptr, uint32_t size, int tag, uint32_t remote_offset, uint32_t *request) {
	if(__photon_backend->initialized() != PHOTON_OK) {
		init_err();
		return PHOTON_ERROR_NOINIT;
	}

	return __photon_backend->post_os_get(proc, ptr, size, tag, remote_offset, request);
}

int photon_send_FIN(int proc) {
	if(__photon_backend->initialized() != PHOTON_OK) {
		init_err();
		return PHOTON_ERROR_NOINIT;
	}

	return __photon_backend->send_FIN(proc);
}

int photon_wait_any(int *ret_proc, uint32_t *ret_req) {
	if(__photon_backend->initialized() != PHOTON_OK) {
		init_err();
		return PHOTON_ERROR_NOINIT;
	}

#ifdef PHOTON_MULTITHREADED
	// TODO: These can probably be implemented by having
	//	 a condition var for the unreaped lists
	return PHOTON_ERROR;
#else
	return __photon_backend->wait_any(ret_proc, ret_req);
#endif
}

int photon_wait_any_ledger(int *ret_proc, uint32_t *ret_req) {
	if(__photon_backend->initialized() != PHOTON_OK) {
		init_err();
		return PHOTON_ERROR_NOINIT;
	}

#ifdef PHOTON_MULTITHREADED
	return PHOTON_ERROR;
#else
	return __photon_backend->wait_any_ledger(ret_proc, ret_req);
#endif
}

int photon_probe_ledger(int proc, int *flag, int type, photonStatus status) {
	if(__photon_backend->initialized() != PHOTON_OK) {
		init_err();
		return PHOTON_ERROR_NOINIT;
	}

	return __photon_backend->probe_ledger(proc, flag, type, status);
}

/*
  int photon_post_recv(int proc, char *ptr, uint32_t size, uint32_t *request) {

  }

  int photon_post_send(int proc, char *ptr, uint32_t size, uint32_t *request) {

  }

  int photon_wait_remaining() {

  }

  int photon_wait_remaining_ledger() {

  }
*/

