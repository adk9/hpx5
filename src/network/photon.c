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

#include "hpx/action.h"
#include "hpx/network.h"
#include "hpx/parcel.h"
#include "hpx/network/mpi.h"
#include "hpx/network/photon.h"

#include <mpi.h>
#include <photon.h>

#define PHOTON_TAG 0xdead

/* Photon network operations */
network_ops_t photon_ops = {
  .init     = _init_photon,
  .finalize = _finalize_photon,
  .progress = _progress_photon,
  .probe    = _probe_photon,
  .send     = _put_photon,
  .recv     = _get_photon,
  .test     = _test_photon,
  .put      = _put_photon,
  .get      = _get_photon,
  .pin      = _pin_photon,
  .unpin    = _unpin_photon,
};

int _eager_threshold_PHOTON = _EAGER_THRESHOLD_PHOTON_DEFAULT;
int _rank_photon;
int _size_photon;

static char* ETH_DEV_ROCE0 = "roce0";
static char* IB_DEV_MLX4_1 = "mlx4_1";

uint32_t _get_rank_photon() {
  return (uint32_t)_rank_photon;
}

uint32_t _get_size_photon() {
  return (uint32_t)_size_photon;
}

/* If using Photon, call this instead of _init_mpi */
int _init_photon(void) {
  int retval;
  int temp;
  int thread_support_provided;

  /* runtime configuration options */
  char* eth_dev;
  char* ib_dev;
  int use_cma;

  retval = HPX_ERROR;

  /* TODO: see if we really need thread multiple */
  //  temp = MPI_Init_thread(0, NULL, MPI_THREAD_MULTIPLE, &thread_support_provided); /* TODO: should be argc and argv if possible */
  temp = MPI_Init(0, NULL); /* TODO: should be argc and argv if possible */
  if (temp == MPI_SUCCESS)
    retval = 0;
  else {
    __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */
    goto error;
  }

  MPI_Comm_rank(MPI_COMM_WORLD, &_rank_photon);
  MPI_Comm_size(MPI_COMM_WORLD, &_size_photon);

  // TODO: make eth_dev and ib_dev runtime configurable!
  eth_dev = getenv("HPX_USE_ETH_DEV");
  ib_dev = getenv("HPX_USE_IB_DEV");

  if (eth_dev == NULL)
    eth_dev = ETH_DEV_ROCE0;
  if (ib_dev == NULL)
    ib_dev = IB_DEV_MLX4_1;
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
	  .backend = "verbs"
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
int _finalize_photon(void) {
  int retval;
  int temp;
  retval = HPX_ERROR;

  temp = MPI_Finalize();

  if (temp != MPI_SUCCESS) {
    __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */
    goto error;
  }

  temp = photon_finalize();
  if (temp == 0)
    retval = 0;

 error:
  return retval;
}


/* just tell the other side we have a buffer ready to be retrieved */
int _put_photon(int dst, void* buffer, size_t len, network_request_t *request) {
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

int _get_photon(int src, void* buffer, size_t len, network_request_t *request) {
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

int _test_photon(network_request_t *request, int *flag, network_status_t *status) {
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

int _send_photon(int peer, void *payload, size_t len, network_request_t *request) {

	return HPX_SUCCESS;
}

int _recv_photon(int src, void *buffer, size_t len, network_request_t *request) {
	
	return HPX_SUCCESS;
}

int _probe_photon(int src, int *flag, network_status_t* status) {
	int retval;
	int temp;
	int phot_src;
	struct photon_status_t stat;
	
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

void _progress_photon(void *data) {
	
}

/* pin memory for put/get */
int _pin_photon(void* buffer, size_t len) {
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
int _unpin_photon(void* buffer, size_t len) {
  int temp;
  int retval;

  retval = HPX_ERROR;

  printf("in UNPIN\n");

  temp = photon_unregister_buffer(buffer, len);
  if (temp != 0) {
    __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */
    goto error;
  }

 error:
  return retval;
}
