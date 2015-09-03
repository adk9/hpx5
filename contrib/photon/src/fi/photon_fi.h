#ifndef PHOTON_FI_H
#define PHOTON_FI_H

#include <stdint.h>
#include <rdma/fabric.h>

#include "libphoton.h"
#include "photon_backend.h"
#include "photon_fi_buffer.h"

#define PHOTON_FI_PUT_ALIGN 1
#define PHOTON_FI_GET_ALIGN 1

#ifndef FT_FIVERSION
#define FT_FIVERSION FI_VERSION(FI_MAJOR_VERSION,FI_MINOR_VERSION)
#endif

// for RMA ops we want to be able to select fi_writedata, but there is no
// constant in libfabric for this
enum ft_rma_opcodes {
	FT_RMA_READ = 1,
	FT_RMA_WRITE,
	FT_RMA_WRITEDATA,
};

#define MAX_CQ_POLL    8

extern struct photon_backend_t photon_fi_backend;

#endif
