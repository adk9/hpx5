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
	
	
	return PHOTON_OK;
}

int __ugni_connect_peers(ugni_cnct_ctx *ctx) {


	return PHOTON_OK;
}
