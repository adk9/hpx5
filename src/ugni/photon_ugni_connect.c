#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>

#include "libphoton.h"
#include "photon_ugni_connect.h"
#include "logging.h"

#define BIND_ID_MULTIPLIER       100
#define CACHELINE_MASK           0x3F   /* 64 byte cacheline */
#define CDM_ID_MULTIPLIER        1000
#define LOCAL_EVENT_ID_BASE      10000000
#define NUMBER_OF_TRANSFERS      10
#define POST_ID_MULTIPLIER       1000
#define REMOTE_EVENT_ID_BASE     11000000
#define GEMINI_DEVICE_ID         0x0
#define MAX_CQ_ENTRIES           1000

#ifdef HAVE_DEBUG
int v_option = 3;
#else
int v_option = 0;
#endif

#include "utility_functions.h"

extern int _photon_myrank;
extern int _photon_nproc;
extern int _photon_forwarder;

int __ugni_init_context(ugni_cnct_ctx *ctx) {
	unsigned local_address;
	int address, cookie;
	int status;
	/* GNI_CDM_MODE_BTE_SINGLE_CHANNEL | GNI_CDM_MODE_DUAL_EVENTS | GNI_CDM_MODE_FMA_SHARED */
	int modes = 0;
	uint8_t ptag;

	address = get_gni_nic_address(GEMINI_DEVICE_ID);
	ptag = get_ptag();
	cookie = get_cookie();
	
	ctx->cdm_id = _photon_myrank * CDM_ID_MULTIPLIER + 1;

	status = GNI_CdmCreate(ctx->cdm_id, ptag, cookie, modes, &(ctx->cdm_handle));
    if (status != GNI_RC_SUCCESS) {
		dbg_err("GNI_CdmCreate ERROR status: %s (%d)", gni_err_str[status], status);
        goto error_exit;
    }
	
	dbg_info("GNI_CdmCreate inst_id: %i ptag: %u cookie: 0x%x", ctx->cdm_id, ptag, cookie);

	status = GNI_CdmAttach(ctx->cdm_handle, GEMINI_DEVICE_ID, &local_address, &ctx->nic_handle);
    if (status != GNI_RC_SUCCESS) {
        dbg_err("GNI_CdmAttach ERROR status: %s (%d)", gni_err_str[status], status);
        goto error_exit;
    }
	
	dbg_info("Attached to GEMINI PE: %d", local_address);

	/* setup completion queue for local events */
    status = GNI_CqCreate(ctx->nic_handle, MAX_CQ_ENTRIES, 0, GNI_CQ_NOBLOCK, NULL, NULL, &(ctx->local_cq_handle));
    if (status != GNI_RC_SUCCESS) {
		dbg_err("GNI_CqCreate local_cq ERROR status: %s (%d)\n", gni_err_str[status], status);
        goto error_exit;
    }

	/* setup completion queue for remote memory events */
    status = GNI_CqCreate(ctx->nic_handle, MAX_CQ_ENTRIES, 0, GNI_CQ_NOBLOCK, NULL, NULL, &(ctx->remote_cq_handle));
    if (status != GNI_RC_SUCCESS) {
		dbg_err("GNI_CqCreate remote_cq ERROR status: %s (%d)\n", gni_err_str[status], status);
        goto error_exit;
    }

	return PHOTON_OK;

 error_exit:
	return PHOTON_ERROR;
}

int __ugni_connect_peers(ugni_cnct_ctx *ctx) {


	return PHOTON_OK;
}
