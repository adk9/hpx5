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

#include "photon_ugni.h"
#include "logging.h"

static int __initialized = 0;

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
	int status;
	unsigned address, cpu_id;
	int device_id = 0;

	status = GNI_CdmGetNicAddress(device_id, &address, &cpu_id);
	if (status != GNI_RC_SUCCESS) {
		fprintf(stdout,
				"GNI_CdmGetNicAddress ERROR status: %s (%d)\n", gni_err_str[status], status);
		abort();
	}

	printf("DEV_ADDR: %u, CPU_ID: %u\n", address, cpu_id);
    printf("PMI_GNI_DEV_ID: %s\n", getenv("PMI_GNI_DEV_ID"));
	printf("PMI_GNI_LOC_ADDR: %s\n", getenv("PMI_GNI_LOC_ADDR"));
	
	buffertable_init(193);

	__initialized = 1;

	return PHOTON_OK;
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
