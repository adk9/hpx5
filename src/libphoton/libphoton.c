#include <strings.h>
#include "libphoton.h"
#include "logging.h"

/* Globals */
int _photon_nproc;
int _photon_myrank;

#ifdef DEBUG
int _photon_start_debugging=1;
#endif

#if defined(DEBUG) || defined(CALLTRACE)
FILE *_phot_ofp;
#endif

#ifdef HAVE_VERBS
#include "verbs.h"
#endif


int photon_init(photonConfig cfg) {
    
	if (cfg->backend != NULL) {
		if (!strncasecmp(cfg->backend, "verbs", 10)) {
#ifdef HAVE_VERBS
			__photon_backend = &photon_verbs_backend;
#else
			goto error_exit;
#endif
		}
		else if (!strncasecmp(cfg->backend, "ugni", 10)) {
#ifdef HAVE_UGNI
			__photon_backend = &photon_ugni_backend;
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
    if(__photon_backend->initialized() != PHOTON_OK) {
		init_err();
		return PHOTON_ERROR_NOINIT;
    }
    
    return __photon_backend->register_buffer(buffer, buffer_size);
}

int photon_unregister_buffer(char *buffer, int size) {
    if(__photon_backend->initialized() != PHOTON_OK) {
		init_err();
        return PHOTON_ERROR_NOINIT;
    }
    
    return __photon_backend->unregister_buffer(buffer, size);
}


int photon_test(uint32_t request, int *flag, int *type, MPI_Status *status) {
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

