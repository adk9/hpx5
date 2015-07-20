#ifndef PHOTON_VERBS
#define PHOTON_VERBS

#include <stdint.h>
#include <pthread.h>
#include <netinet/in.h>
#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>

#include "libphoton.h"
#include "photon_backend.h"

#include "verbs_buffer.h"

#define MAX_CQ_POLL   8
#define MAX_QP        1

#define PHOTON_VERBS_PUT_ALIGN 1
#define PHOTON_VERBS_GET_ALIGN 1

#define VERBS_DEF_NUM_SRQ      0

extern struct photon_backend_t photon_verbs_backend;

#endif
