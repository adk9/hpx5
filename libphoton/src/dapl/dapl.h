#ifndef DAPL_H
#define DAPL_H

#include <stdint.h>
#include <pthread.h>

#include "squeue.h"
#include "dapl_buffer.h"
#include "dapl_rdma_info_ledger.h"
#include "dapl_rdma_FIN_ledger.h"

#ifdef WITH_XSP
#include "photon_xsp.h"
#include "libxsp_client.h"
#endif

#define DEF_QUEUE_LENGTH 		5*10240
#define DEF_NUM_REQUESTS 		5*10240
#define LEDGER_SIZE			10240

#define NULL_COOKIE			0x0

#define LEDGER				1
#define EVQUEUE				2

#define REQUEST_PENDING			1
#define REQUEST_FAILED			2
#define REQUEST_COMPLETED		3

typedef struct proc_info_t {
   DAT_IA_ADDRESS_PTR remote_addr;

   DAT_EP_HANDLE   clnt_ep;
   uint32_t        clnt_segment_length;

   DAT_EP_HANDLE   srvr_ep;
   DAT_PSP_HANDLE  srvr_psp;
   DAT_RMR_TRIPLET srvr_rdma_rmr;
   uint32_t        srvr_segment_length;

   struct sockaddr_storage sa;
   socklen_t sa_len;

   DAT_CONN_QUAL clnt_conn_qual;
   DAT_CONN_QUAL srvr_conn_qual;

   dapl_ri_ledger_t *local_snd_info_ledger;
   dapl_ri_ledger_t *remote_snd_info_ledger;
   dapl_ri_ledger_t *local_rcv_info_ledger;
   dapl_ri_ledger_t *remote_rcv_info_ledger;
   dapl_rdma_FIN_ledger_t *local_FIN_ledger;
   dapl_rdma_FIN_ledger_t *remote_FIN_ledger;

   dapl_remote_buffer_t *curr_remote_buffer;

#ifdef WITH_XSP
   libxspSess *sess;
   PhotonIOInfo *io_info;
#endif
} ProcessInfo;

typedef struct dapl_req {
	LIST_ENTRY(dapl_req) list;
	uint32_t id;
	int state;
	int type;
	int proc;
	int tag;
	pthread_mutex_t mtx;
	pthread_cond_t completed;
} dapl_req_t;

struct mem_register_req {
	SLIST_ENTRY(mem_register_req) list;
	char *buffer;
	int buffer_size;
};

void log_dat_err(const DAT_RETURN status, const char *fmt, ...);
void log_dto_err(const DAT_DTO_COMPLETION_STATUS status, const char *fmt, ...);
const char *str_dto_completion_status(const DAT_DTO_COMPLETION_STATUS status);
int dapl_create_listener(DAT_IA_HANDLE ia, DAT_PZ_HANDLE pz, DAT_CONN_QUAL *conn_qual, DAT_EVD_HANDLE cevd, DAT_EVD_HANDLE dto_evd, DAT_EP_HANDLE *ret_ep, DAT_PSP_HANDLE *ret_psp);
int dat_client_connect(DAT_IA_HANDLE ia, DAT_PZ_HANDLE pz, DAT_IA_ADDRESS_PTR remote_addr, DAT_CONN_QUAL conn_qual, DAT_EVD_HANDLE cevd, DAT_EVD_HANDLE dto_evd, DAT_EP_HANDLE *ret_ep);

enum dapl_dto_types {DAPL_ERROR, DAPL_SEND, DAPL_RECV, DAPL_RDMA_SEND, DAPL_SEND_RECV_BUFFER};

#endif
