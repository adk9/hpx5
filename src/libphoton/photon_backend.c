#include "photon_backend.h"

extern int _photon_nproc;
extern int _photon_myrank;

struct photon_backend_t photon_default_backend = {
	.initialized = _photon_initialized,
	.init = _photon_init,
	.finalize = _photon_finalize,
	.register_buffer = _photon_register_buffer,
	.unregister_buffer = _photon_unregister_buffer,
	.test = _photon_test,
	.wait = _photon_wait,
	.wait_ledger = _photon_wait_ledger,
	.post_recv_buffer_rdma = _photon_post_recv_buffer_rdma,
	.post_send_buffer_rdma = _photon_post_send_buffer_rdma,
	.wait_recv_buffer_rdma = _photon_wait_recv_buffer_rdma,
	.wait_send_buffer_rdma = _photon_wait_send_buffer_rdma,
	.wait_send_request_rdma = _photon_wait_send_request_rdma,
	.post_os_put = _photon_post_os_put,
	.post_os_get = _photon_post_os_get,
	.send_FIN = _photon_send_FIN,
	.wait_any = _photon_wait_any,
	.wait_any_ledger = _photon_wait_any_ledger
};

int _photon_initialized() {
	return PHOTON_OK;
}

int _photon_init(photonConfig cfg) {
	_photon_nproc = 1;
	_photon_myrank = 0;
	return PHOTON_OK;
}

int _photon_finalize() {
	return PHOTON_OK;
}

int _photon_register_buffer(char *buffer, int buffer_size) {
	return PHOTON_OK;
}

int _photon_unregister_buffer(char *buffer, int size) {
	return PHOTON_OK;
}

int _photon_test(uint32_t request, int *flag, int *type, void *status) {
	return PHOTON_OK;
}

int _photon_wait(uint32_t request) {
	return PHOTON_OK;
}

int _photon_wait_ledger(uint32_t request) {
	return PHOTON_OK;
}

int _photon_post_recv_buffer_rdma(int proc, char *ptr, uint32_t size, int tag, uint32_t *request) {
	return PHOTON_OK;
}

int _photon_post_send_buffer_rdma(int proc, char *ptr, uint32_t size, int tag, uint32_t *request) {
	return PHOTON_OK;
}

int _photon_post_send_request_rdma(int proc, uint32_t size, int tag, uint32_t *request) {
	return PHOTON_OK;
}

int _photon_wait_recv_buffer_rdma(int proc, int tag) {
	return PHOTON_OK;
}

int _photon_wait_send_buffer_rdma(int proc, int tag) {
	return PHOTON_OK;
}

int _photon_wait_send_request_rdma(int tag) {
	return PHOTON_OK;
}

int _photon_post_os_put(int proc, char *ptr, uint32_t size, int tag, uint32_t remote_offset, uint32_t *request) {
	return PHOTON_OK;
}

int _photon_post_os_get(int proc, char *ptr, uint32_t size, int tag, uint32_t remote_offset, uint32_t *request) {
	return PHOTON_OK;
}

int _photon_send_FIN(int proc) {
	return PHOTON_OK;
}

int _photon_wait_any(int *ret_proc, uint32_t *ret_req) {
	return PHOTON_OK;
}

int _photon_wait_any_ledger(int *ret_proc, uint32_t *ret_req) {
	return PHOTON_OK;
}
