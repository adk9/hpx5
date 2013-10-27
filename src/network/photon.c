/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Photon Network Interface 
  photon.c

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <photon.h>

#include "hpx/action.h"
#include "hpx/init.h"
#include "hpx/parcel.h"

#include "bootstrap/bootstrap.h"
#include "network.h"

#include "network_mpi.h"
#include "network_photon.h"

#define PHOTON_TAG 0xdead

/* Photon network operations */
network_ops_t photon_ops = {
  .init     = init_photon,
  .finalize = finalize_photon,
  .progress = progress_photon,
  .probe    = probe_photon,
  .send     = put_photon,
  .recv     = get_photon,
  .sendrecv_test     = test_photon,
  .put      = put_photon,
  .get      = get_photon,
  .putget_test     = test_photon,
  .pin      = pin_photon,
  .unpin    = unpin_photon,
  .phys_addr= phys_addr_photon,
};

int eager_threshold_PHOTON = EAGER_THRESHOLD_PHOTON_DEFAULT;
int _rank_photon;
int _size_photon;

static char* ETH_DEV_ROCE0 = "roce0";
static char* IB_DEV_MLX4_1 = "mlx4_1";
static char* BACKEND_UGNI = "ugni";
//static char* BACKEND_VERBS = "verbs";

/* If using Photon, call this instead of _init_mpi */
int init_photon(void) {
  int retval;
  int temp;
  //  int thread_support_provided;

  /* runtime configuration options */
  char* eth_dev;
  char* ib_dev;
  char* backend;
  int use_cma;

  retval = HPX_ERROR;

  /* TODO: see if we really need thread multiple */
  //  temp = MPI_Init_thread(0, NULL, MPI_THREAD_MULTIPLE, &thread_support_provided); /* TODO: should be argc and argv if possible */
  MPI_Initialized(&retval);
  if (!retval) {
    temp = MPI_Init(0, NULL); /* TODO: should be argc and argv if possible */
    if (temp == MPI_SUCCESS)
      retval = 0;
    else {
      __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */
      goto error;
    }
  }

  _rank_photon = bootmgr->get_rank();
  _size_photon = bootmgr->size();

  // TODO: make eth_dev and ib_dev runtime configurable!
  eth_dev = getenv("HPX_USE_ETH_DEV");
  ib_dev = getenv("HPX_USE_IB_DEV");
  backend = getenv("HPX_USE_BACKEND");

  if (eth_dev == NULL)
    eth_dev = ETH_DEV_ROCE0;
  if (ib_dev == NULL)
    ib_dev = IB_DEV_MLX4_1;
  if (backend == NULL)
    backend = BACKEND_UGNI;
  if(getenv("HPX_USE_CMA") == NULL)
    use_cma = 1;
  else
    use_cma = atoi(getenv("HPX_USE_CMA"));

  struct photon_config_t photon_conf = {
	  .meta_exch = PHOTON_EXCH_MPI,
	  .nproc = _size_photon,
	  .address = _rank_photon,
	  .comm = MPI_COMM_WORLD,
	  .use_forwarder = 0,
	  .use_cma = use_cma,
	  .eth_dev = eth_dev,
	  .ib_dev = ib_dev,
	  .ib_port = 1,
	  .backend = backend
  };

  temp =  photon_init(&photon_conf);
  if (temp == 0)
    retval = HPX_SUCCESS;
  else
    __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */

 error:
  return retval;
}

/* If you're using Photon, call this instead of _finalize_mpi */
int finalize_photon(void) {
  int retval;
  int temp;
  retval = HPX_ERROR;

  MPI_Finalized(&retval);
  if (!retval) {
    temp = MPI_Finalize();
    if (temp != MPI_SUCCESS) {
      __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */
      goto error;
    }
  }

  temp = photon_finalize();
  if (temp == 0)
    retval = 0;

 error:
  return retval;
}


/* just tell the other side we have a buffer ready to be retrieved */
int put_photon(int dst, void* buffer, size_t len, network_request_t *request) {
  int temp;
  int retval;

  retval = HPX_ERROR;

  if (len > UINT32_MAX) {
    __hpx_errno = HPX_ERROR;
    retval = HPX_ERROR;
    goto error;
  }

  temp = photon_post_send_buffer_rdma(dst, buffer, (uint32_t)len, PHOTON_TAG, &(request->photon));
  if (temp != 0) {
    __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */
    goto error;
  }

 error:
  return retval;
}

int get_photon(int src, void* buffer, size_t len, network_request_t *request) {
  int temp;
  int retval;

  retval = HPX_ERROR;
  if (len > UINT32_MAX) {
    __hpx_errno = HPX_ERROR;
    retval = HPX_ERROR;    
    goto error;
  }

  /* make sure we have remote buffer metadata */
  temp = photon_wait_send_buffer_rdma(src, PHOTON_TAG);
  if (temp != 0) {
	  __hpx_errno = HPX_ERROR;
	  goto error;
  }
  /* get the remote buffer */
  temp = photon_post_os_get(src, buffer, (uint32_t)len, PHOTON_TAG, 0, &(request->photon));
  if (temp != 0) {
    __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */
    goto error;
  }
  /* tell the source ledger saying we retrieved its buffer */
  temp = photon_send_FIN(src);
  if (temp != 0) {
    __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */
    goto error;
  }

 error:
  return retval;
}

int test_photon(network_request_t *request, int *flag, network_status_t *status) {
  int retval;
  int temp;
  struct photon_status_t stat;
  int type; /* 0=RDMA completion event, 1=ledger entry */

  retval = HPX_ERROR;

  if (status == NULL) {
    temp = photon_test((request->photon), flag, &type, &stat);
  }
  else {
    temp = photon_test((request->photon), flag, &type, &(status->photon));
	status->source = (int)status->photon.src_addr;
  }

  if (temp == 0)
    retval = 0;
  else
    __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */

  return retval;  
}

int _send_parcel_photon(hpx_locality_t *loc, hpx_parcel_t *parc) {
	printf("in send parcel\n");
	return HPX_SUCCESS;
}

int send_photon(int peer, void *payload, size_t len, network_request_t *request) {

	return HPX_SUCCESS;
}

int recv_photon(int src, void *buffer, size_t len, network_request_t *request) {
	
	return HPX_SUCCESS;
}

int probe_photon(int src, int *flag, network_status_t* status) {
	int retval;
	int temp;
	int phot_src;
	// struct photon_status_t stat;
	
	retval = HPX_ERROR;

	if (src == NETWORK_ANY_SOURCE) {
		phot_src = PHOTON_ANY_SOURCE;
	}
	else {
		phot_src = src;
	}
	
	if (status == NULL) {
		temp = photon_probe_ledger(phot_src, flag, PHOTON_SEND_LEDGER, &status->photon);
	}
	else {
		temp = photon_probe_ledger(phot_src, flag, PHOTON_SEND_LEDGER, &status->photon);
		status->source = (int)status->photon.src_addr;
	}

	if (temp == 0)
		retval = HPX_SUCCESS;
	else
		__hpx_errno = HPX_ERROR;

	status->count = status->photon.size;

	return retval;
}

void progress_photon(void *data) {	
}

/* pin memory for put/get */
int pin_photon(void* buffer, size_t len) {
  int temp;
  int retval;

  retval = HPX_ERROR;

  temp = photon_register_buffer(buffer, len);
  if (temp != 0) {
    __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */
    goto error;
  }

 error:
  return retval;
}

/* unpin memory for put/get */
int unpin_photon(void* buffer, size_t len) {
  int temp;
  int retval;

  retval = HPX_ERROR;

  temp = photon_unregister_buffer(buffer, len);
  if (temp != 0) {
    __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */
    goto error;
  }

 error:
  return retval;
}

/* Return the physical network ID of the current process */
int phys_addr_photon(hpx_locality_t *l) {
  int ret;
  ret = HPX_ERROR;

  if (!l) {
    /* TODO: replace with more specific error */
    __hpx_errno = HPX_ERROR;
    return ret;
  }

  l->rank = bootmgr->get_rank();
  return 0;
}
