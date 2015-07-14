#ifndef PHOTON_FI
#define PHOTON_FI

#include <stdint.h>

#include "rdma/fabric.h"

#include "libphoton.h"
#include "photon_backend.h"
#include "photon_rdma_INFO_ledger.h"
#include "photon_rdma_ledger.h"

#define MAX_CQ_POLL    8

extern struct photon_backend_t photon_fi_backend;

#endif
