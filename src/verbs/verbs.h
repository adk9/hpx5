#ifndef PHOTON_VERBS
#define PHOTON_VERBS

#include <stdint.h>
#include <pthread.h>
#include <netinet/in.h>
#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>

#include "squeue.h"
#include "verbs_buffer.h"
#include "verbs_rdma_INFO_ledger.h"
#include "verbs_rdma_FIN_ledger.h"

#include "photon_backend.h"

#ifdef WITH_XSP
#include "photon_xsp.h"
#include "libxsp_client.h"
#endif

#define DEF_QUEUE_LENGTH 		5*10240
#define DEF_NUM_REQUESTS 		5*10240
#define LEDGER_SIZE			    10240

#define NULL_COOKIE			    0x0

#define LEDGER				    1
#define EVQUEUE				    2

#define REQUEST_PENDING			1
#define REQUEST_FAILED			2
#define REQUEST_COMPLETED		3

#define MAX_QP 1

typedef struct proc_info_t {
	struct ibv_qp     *qp[MAX_QP];
	int                psn;
	int                num_qp;

	// for RDMA CMA connections
	struct rdma_cm_id *cm_id;
	void              *local_priv;
	uint64_t           local_priv_size;  
	void              *remote_priv;
	uint64_t           remote_priv_size;

	// Conduit independent (almost) part
	verbs_ri_ledger_t *local_snd_info_ledger;
	verbs_ri_ledger_t *remote_snd_info_ledger;
	verbs_ri_ledger_t *local_rcv_info_ledger;
	verbs_ri_ledger_t *remote_rcv_info_ledger;
	verbs_rdma_FIN_ledger_t *local_FIN_ledger;
	verbs_rdma_FIN_ledger_t *remote_FIN_ledger;

	verbs_remote_buffer_t *curr_remote_buffer;

#ifdef WITH_XSP
	libxspSess *sess;
	PhotonIOInfo *io_info;
#endif
} ProcessInfo;

typedef struct verbs_req {
	LIST_ENTRY(verbs_req) list;
	uint32_t id;
	int state;
	int type;
	int proc;
	int tag;
	pthread_mutex_t mtx;
	pthread_cond_t completed;
} verbs_req_t;

struct mem_register_req {
	SLIST_ENTRY(mem_register_req) list;
	char *buffer;
	int buffer_size;
};

extern struct photon_backend_t photon_verbs_backend;

#endif
