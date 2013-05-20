#ifndef photon_H
#define photon_H

#include <stdint.h>

#ifdef WITH_XSP
#include "photon_xsp.h"
#endif

int photon_init(int nproc, int rank, MPI_Comm comm);
int photon_finalize();

int photon_register_buffer(char *buffer, int buffer_size);
int photon_unregister_buffer(char *buffer, int size);

int photon_post_recv_buffer_rdma(int proc, char *ptr, uint32_t size, int tag, uint32_t *request);
int photon_post_send_buffer_rdma(int proc, char *ptr, uint32_t size, int tag, uint32_t *request);
int photon_post_send_request_rdma(int proc, uint32_t size, int tag, uint32_t *request);
int photon_wait_recv_buffer_rdma(int proc, int tag);
int photon_wait_send_buffer_rdma(int proc, int tag);
int photon_wait_send_request_rdma(int tag);
int photon_post_os_put(int proc, char *ptr, uint32_t size, int tag, uint32_t remote_offset, uint32_t *request);
int photon_post_os_get(int proc, char *ptr, uint32_t size, int tag, uint32_t remote_offset, uint32_t *request);
int photon_send_FIN(int proc);
int photon_test(uint32_t request, int *flag, int *type, MPI_Status *status);

int photon_wait(uint32_t request);
int photon_wait_ledger(uint32_t request);

int photon_wait_any(int *ret_proc, uint32_t *ret_req);
int photon_wait_any_ledger(int *ret_proc, uint32_t *ret_req);

// The following 4 functions are implemented over DAPL
// only and will not be supported in the future.
int photon_post_recv(int proc, char *ptr, uint32_t size, uint32_t *request);
int photon_post_send(int proc, char *ptr, uint32_t size, uint32_t *request);
int photon_wait_remaining();
int photon_wait_remaining_ledger();

#endif
