#ifndef PHOTON_VERBS
#define PHOTON_VERBS

#include <stdint.h>
#include <pthread.h>
#include <netinet/in.h>
#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>

#include "libphoton.h"
#include "photon_backend.h"
#include "photon_rdma_INFO_ledger.h"
#include "photon_rdma_FIN_ledger.h"

#include "verbs_buffer.h"
#include "squeue.h"

#ifdef WITH_XSP
#include "photon_xsp.h"
#include "libxsp_client.h"
#endif

#define DEF_QUEUE_LENGTH 		5*10240
#define DEF_NUM_REQUESTS 		5*10240
#define LEDGER_SIZE			    10240

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
	photonRILedger  local_snd_info_ledger;
	photonRILedger  remote_snd_info_ledger;
	photonRILedger  local_rcv_info_ledger;
	photonRILedger  remote_rcv_info_ledger;
	photonFINLedger local_FIN_ledger;
	photonFINLedger remote_FIN_ledger;

	photonRemoteBuffer curr_remote_buffer;

#ifdef WITH_XSP
	libxspSess *sess;
	PhotonIOInfo *io_info;
#endif
} ProcessInfo;

extern struct photon_backend_t photon_verbs_backend;

#endif
