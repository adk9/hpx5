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
#include "utility_functions.h"
#include "logging.h"

#define BIND_ID_MULTIPLIER       100
#define CDM_ID_MULTIPLIER        1000
#define POST_ID_MULTIPLIER       1000
#define GEMINI_DEVICE_ID         0x0
#define MAX_CQ_ENTRIES           1000

extern int _photon_myrank;
extern int _photon_nproc;
extern int _photon_forwarder;

int __ugni_connect_endpoints(ugni_cnct_ctx *ctx, ugni_cnct_info *local_info, ugni_cnct_info *remote_info);

int __ugni_init_context(ugni_cnct_ctx *ctx) {
	unsigned local_address;
	int cookie, status;
	/* GNI_CDM_MODE_BTE_SINGLE_CHANNEL | GNI_CDM_MODE_DUAL_EVENTS | GNI_CDM_MODE_FMA_SHARED */
	int modes = 0;
	uint8_t ptag;

	ptag = get_ptag();
	cookie = get_cookie();
	
	ctx->cdm_id = _photon_myrank * CDM_ID_MULTIPLIER + CDM_ID_MULTIPLIER;

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

	/* setup completion queue for remote memory events
    status = GNI_CqCreate(ctx->nic_handle, MAX_CQ_ENTRIES, 0, GNI_CQ_NOBLOCK, NULL, NULL, &(ctx->remote_cq_handle));
    if (status != GNI_RC_SUCCESS) {
		dbg_err("GNI_CqCreate remote_cq ERROR status: %s (%d)\n", gni_err_str[status], status);
        goto error_exit;
    }
	*/

	ctx->ep_handles = (gni_ep_handle_t *)calloc(_photon_nproc, sizeof(gni_ep_handle_t));
	if (!ctx->ep_handles) {
		dbg_err("Could not allocate endpoint array");
		goto error_exit;
	}

	return PHOTON_OK;

 error_exit:
	return PHOTON_ERROR;
}

int __ugni_connect_peers(ugni_cnct_ctx *ctx) {
	ugni_cnct_info *remote_info, *curr_info;
	ugni_cnct_info local_info;
	unsigned int *remote_lids;
	unsigned int *remote_ips;
	struct ifaddrs *ifaddr, *ifa;
	int i, rc;
	MPI_Comm _photon_comm = __photon_config->comm;

	remote_info = (ugni_cnct_info *)malloc(_photon_nproc * sizeof(ugni_cnct_info));
	if(!remote_info) {
		goto error_exit;
	}

	remote_ips = (unsigned int *)malloc(_photon_nproc * sizeof(unsigned int));
	if (!remote_ips) {
		goto error_exit;
	}

	remote_lids = (unsigned int *)malloc(_photon_nproc * sizeof(unsigned int));
	if (!remote_lids) {
		goto error_exit;
	}

	memset(remote_info, 0, _photon_nproc*sizeof(ugni_cnct_info));
	memset(remote_ips, 0, _photon_nproc*sizeof(unsigned int));
	memset(remote_lids, 0, _photon_nproc*sizeof(unsigned int));

	if (getifaddrs(&ifaddr) == -1) {
		dbg_info("Cannot get interface addrs");
		ifa = NULL;
	}
	else {
		for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
			if (ifa->ifa_addr == NULL)
				continue;
			
			if (!strcmp(ifa->ifa_name, ctx->gemini_dev) &&
				ifa->ifa_addr->sa_family == AF_INET) {
				break;
			}
		}
	}
	
	if (!ifa) {
		dbg_info("Did not find interface info for %s\n", ctx->gemini_dev);
		local_info.ip.s_addr = 0x0;
	}
	else {
		local_info.ip = ((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
		dbg_info("Found local IP: %s", inet_ntoa(local_info.ip));
	}
	
	local_info.lid = get_gni_nic_address(GEMINI_DEVICE_ID);

	rc = MPI_Allgather(&local_info.lid, 1, MPI_UNSIGNED, remote_lids, 1, MPI_UNSIGNED, _photon_comm);
	if (rc != MPI_SUCCESS) {
		dbg_err("Could not allgather NIC IDs");
		goto error_exit_free;
	}

	rc = MPI_Allgather(&local_info.ip.s_addr, 1, MPI_UNSIGNED, remote_ips, 1, MPI_UNSIGNED, _photon_comm);
	if (rc != MPI_SUCCESS) {
		dbg_err("Could not allgather NIC IPs");
		goto error_exit_free;
	}

	curr_info = remote_info;
	for (i=0; i<_photon_nproc; i++) {
		curr_info[i].lid = remote_lids[i];
		curr_info[i].ip.s_addr = remote_ips[i];
		dbg_info(">>>> from rank: %d, ID: %u, IP: %s", i, curr_info[i].lid, inet_ntoa(curr_info[i].ip));
	}						 

	if (__ugni_connect_endpoints(ctx, &local_info, remote_info)) {
		dbg_err("Could not connect GEMINI endpoints");
		goto error_exit_free;
	}
	
	free(remote_info);
	free(remote_ips);
	free(remote_lids);
	
	return PHOTON_OK;

 error_exit_free:
	free(remote_info);
	free(remote_ips);
	free(remote_lids);
 error_exit:
	return PHOTON_ERROR;
}

int __ugni_connect_endpoints(ugni_cnct_ctx *ctx, ugni_cnct_info *local_info, ugni_cnct_info *remote_info) {
	int i, status;
	uint32_t bind_id;

	for (i=0; i<_photon_nproc; i++) {
		status = GNI_EpCreate(ctx->nic_handle, ctx->local_cq_handle, &ctx->ep_handles[i]);
		if (status != GNI_RC_SUCCESS) {
			dbg_err("GNI_EpCreate ERROR status: %s (%d)", gni_err_str[status], status);
			goto error_exit;
		}
		dbg_info("GNI_EpCreate remote rank: %4i NIC: %p, CQ: %p, EP: %p", i, ctx->nic_handle,
				 ctx->local_cq_handle, ctx->ep_handles[i]);
		
		
		bind_id = (_photon_myrank * BIND_ID_MULTIPLIER) + BIND_ID_MULTIPLIER + i;
		
		status = GNI_EpBind(ctx->ep_handles[i], remote_info[i].lid, bind_id);
        if (status != GNI_RC_SUCCESS) {
			dbg_err("GNI_EpBind ERROR status: %s (%d)", gni_err_str[status], status);
            goto error_exit;
        }
		dbg_info("GNI_EpBind   remote rank: %4i EP:  %p remote_address: %u, remote_id: %u", i,
				 ctx->ep_handles[i], remote_info[i].lid, bind_id);
	}
	
	return PHOTON_OK;

 error_exit:
	return PHOTON_ERROR;
}
