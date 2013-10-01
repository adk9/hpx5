#ifndef PHOTON_UGNI
#define PHOTON_UGNI

#include <stdint.h>
#include <pthread.h>

#include "gni_pub.h"

#include "libphoton.h"
#include "photon_backend.h"
#include "photon_rdma_INFO_ledger.h"
#include "photon_rdma_FIN_ledger.h"
#include "photon_ugni_buffer.h"

#define DEF_QUEUE_LENGTH 		5*10240
#define DEF_NUM_REQUESTS 		5*10240
#define LEDGER_SIZE			    10240

#define LEDGER				    1
#define EVQUEUE				    2

#define REQUEST_PENDING			1
#define REQUEST_FAILED			2
#define REQUEST_COMPLETED		3

/* use the common set of photon ledgers */
typedef struct ugni_proc_info_t {
	gni_ep_handle_t ep;

	photonRILedger  local_snd_info_ledger;
	photonRILedger  remote_snd_info_ledger;
	photonRILedger  local_rcv_info_ledger;
	photonRILedger  remote_rcv_info_ledger;
	photonFINLedger local_FIN_ledger;
	photonFINLedger remote_FIN_ledger;

	photonRemoteBuffer curr_remote_buffer;
} UgniProcessInfo;

extern struct photon_backend_t photon_ugni_backend;

#endif
