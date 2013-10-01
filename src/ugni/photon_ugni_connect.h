#ifndef PHOTON_UGNI_CONNECT_H
#define PHOTON_UGNI_CONNECT_H

#include <netinet/in.h>
#include "photon_ugni.h"

typedef struct ugni_cnct_ctx_t {
	char               *gemini_dev;
	uint32_t            cdm_id;
	gni_cdm_handle_t    cdm_handle;
	gni_nic_handle_t    nic_handle;
	gni_cq_handle_t     local_cq_handle;
	gni_cq_handle_t     remote_cq_handle;
} ugni_cnct_ctx;

typedef struct ugni_cnct_info_t {
	int lid;
	struct in_addr ip;
} ugni_cnct_info;

int __ugni_init_context(ugni_cnct_ctx *ctx);
int __ugni_connect_peers(ugni_cnct_ctx *ctx);

#endif
