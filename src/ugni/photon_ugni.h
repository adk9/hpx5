#ifndef PHOTON_UGNI
#define PHOTON_UGNI

#include <stdint.h>
#include <pthread.h>

#include "gni_pub.h"

#include "libphoton.h"
#include "photon_backend.h"
#include "photon_rdma_INFO_ledger.h"
#include "photon_rdma_ledger.h"
#include "photon_ugni_buffer.h"

#define MAX_CQ_POLL    8

extern struct photon_backend_t photon_ugni_backend;

#endif
