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
	
	
	return PHOTON_OK;
}

int __ugni_connect_peers(ugni_cnct_ctx *ctx) {


	return PHOTON_OK;
}
