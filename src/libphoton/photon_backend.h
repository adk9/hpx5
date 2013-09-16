#ifndef PHOTON_BACKEND_H
#define PHOTON_BACKEND_H

#include "photon.h"

/* TODO: fix parameters and generalize API */
struct photon_backend_t {
	int (*initialized)(void);
	int (*init)(photonConfig);
	int (*finalize)(void);
	int (*register_buffer)(char *buffer, int size);
	int (*unregister_buffer)(char *buffer, int size);
	int (*test)(uint32_t request, int *flag, int *type, photonStatus status);
	int (*wait)(uint32_t request);
	int (*wait_ledger)(uint32_t request);
	int (*post_recv_buffer_rdma)(int proc, char *ptr, uint32_t size, int tag, uint32_t *request);
	int (*post_send_buffer_rdma)(int proc, char *ptr, uint32_t size, int tag, uint32_t *request);
	int (*post_send_request_rdma)(int proc, uint32_t size, int tag, uint32_t *request);
	int (*wait_recv_buffer_rdma)(int proc, int tag);
	int (*wait_send_buffer_rdma)(int proc, int tag);
	int (*wait_send_request_rdma)(int tag);
	int (*post_os_put)(int proc, char *ptr, uint32_t size, int tag, uint32_t remote_offset, uint32_t *request);
	int (*post_os_get)(int proc, char *ptr, uint32_t size, int tag, uint32_t remote_offset, uint32_t *request);
	int (*send_FIN)(int proc);
	int (*wait_any)(int *ret_proc, uint32_t *ret_req);
	int (*wait_any_ledger)(int *ret_proc, uint32_t *ret_req);
	int (*probe_ledger)(int proc, int *flag, int type, photonStatus status);
};

typedef struct photon_backend_t * photonBackend;

extern struct photon_backend_t photon_default_backend;

/* default backend methods */
int _photon_initialized(void);
int _photon_init(photonConfig cfg);
int _photon_finalize(void);
int _photon_register_buffer(char *buffer, int buffer_size);
int _photon_unregister_buffer(char *buffer, int size);
int _photon_test(uint32_t request, int *flag, int *type, photonStatus status);
int _photon_wait(uint32_t request);
int _photon_wait_ledger(uint32_t request);
int _photon_post_recv_buffer_rdma(int proc, char *ptr, uint32_t size, int tag, uint32_t *request);
int _photon_post_send_buffer_rdma(int proc, char *ptr, uint32_t size, int tag, uint32_t *request);
int _photon_post_send_request_rdma(int proc, uint32_t size, int tag, uint32_t *request);
int _photon_wait_recv_buffer_rdma(int proc, int tag);
int _photon_wait_send_buffer_rdma(int proc, int tag);
int _photon_wait_send_request_rdma(int tag);
int _photon_post_os_put(int proc, char *ptr, uint32_t size, int tag, uint32_t remote_offset, uint32_t *request);
int _photon_post_os_put(int proc, char *ptr, uint32_t size, int tag, uint32_t remote_offset, uint32_t *request);
int _photon_post_os_get(int proc, char *ptr, uint32_t size, int tag, uint32_t remote_offset, uint32_t *request);
int _photon_send_FIN(int proc);
int _photon_wait_any(int *ret_proc, uint32_t *ret_req);
int _photon_wait_any_ledger(int *ret_proc, uint32_t *ret_req);
int _photon_probe_ledger(int proc, int *flag, int type, photonStatus status);

#endif
