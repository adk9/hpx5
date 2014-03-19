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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <photon.h>

#include "network.h"
#include "hpx/error.h"
#include "hpx/globals.h"                        /* bootmgr */
#include "bootstrap.h"                          /* typedef hpx_bootstrap_t */
#include "debug.h"

#define PHOTON_TAG 0xdead
#define EAGER_THRESHOLD_PHOTON_DEFAULT 256

static int init(void);
static int finalize(void);
static void progress(void *data);
static int probe(int source, int* flag, network_status_t* status);
static int put(int dest, void* buffer, size_t len, network_request_t *request);
static int get(int src, void* buffer, size_t len, network_request_t *request);
static int test(network_request_t *request, int *flag, network_status_t *status);
static int test_get(network_request_t *request, int *flag, network_status_t *status);
static int pin(void* buffer, size_t len);
static int unpin(void* buffer, size_t len);
static int phys_addr(hpx_locality_t *l);
static size_t get_network_bytes(size_t n);
static void barrier(void);

/* Photon network operations */
network_ops_t photon_ops = {
  .init              = init,
  .finalize          = finalize,
  .progress          = progress,
  .probe             = probe,
  .send              = put,
  .recv              = get,
  .sendrecv_test     = test,
  .send_test         = test,
  .recv_test         = test_get,
  .put               = put,
  .get               = get,
  .putget_test       = test,
  .put_test          = test,
  .get_test          = test_get,
  .pin               = pin,
  .unpin             = unpin,
  .phys_addr         = phys_addr,
  .get_network_bytes = get_network_bytes,
  .barrier           = barrier
};

/* static int eager_threshold_PHOTON = EAGER_THRESHOLD_PHOTON_DEFAULT; */
static int rank;
static int size;

static char* ETH_DEV_ROCE0 = "roce0";
static char* IB_DEV_MLX4_1 = "mlx4_1";
static char* BACKEND_UGNI = "ugni";
//static char* BACKEND_VERBS = "verbs";

/* If using Photon, call this instead of _init_mpi */
int
init(void)
{
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

  rank = bootmgr->get_rank();
  size = bootmgr->size();

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
	  .nproc = _size,
	  .address = _rank,
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
int
finalize(void)
{
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
int
put(int dst, void* buffer, size_t len, network_request_t *request)
{
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

int
get(int src, void* buffer, size_t len, network_request_t *request)
{
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

#if 0
  temp = photon_send_FIN(src);
  if (temp != 0) {
    __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */
    goto error;
  }
#endif

  retval = HPX_SUCCESS;
 error:
  return retval;
}

/* status may be NULL */
int
test(network_request_t *request, int *flag, network_status_t *status)
{
  int retval;
  int temp;
  struct photon_status_t stat;
  int type = 0; /* 0=RDMA completion event, 1=ledger entry */

  retval = HPX_ERROR;

  if (status == NULL) {
    temp = photon_test((request->photon), flag, &type, &stat);
  }
  else {
    temp = photon_test((request->photon), flag, &type, &(status->photon));
    status->source = (int)status->photon.src_addr;
    status->count = status->photon.size;
  }

  if (temp != 0)
    return (__hpx_errno = HPX_ERROR); /* TODO: replace with more specific error */

  return HPX_SUCCESS;  
}

int 
test_get(network_request_t *request, int *flag, network_status_t *status) {
  int temp;
  struct photon_status_t stat;
  int type = 0; /* 0=RDMA completion event, 1=ledger entry */

  if (status == NULL) {
    temp = photon_test((request->photon), flag, &type, &stat);
  }
  else {
    temp = photon_test((request->photon), flag, &type, &(status->photon));
    status->source = (int)status->photon.src_addr;
    status->count = status->photon.size;
  }

  if (temp == 0 && *flag == 1 && type == 0) {
    temp = photon_send_FIN(status->photon.src_addr);
    if (temp != 0) {
      return (__hpx_errno = HPX_ERROR); /* TODO: replace with more specific error */
    }
  }

  if (temp != 0)
    return (__hpx_errno = HPX_ERROR); /* TODO: replace with more specific error */

  return HPX_SUCCESS;
}


int
probe(int src, int *flag, network_status_t *status)
{
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
		status->count = status->photon.size;
	}

	if (temp == 0)
		retval = HPX_SUCCESS;
	else
		__hpx_errno = HPX_ERROR;

	return retval;
}

void
progress(void *data)
{  
}

/* pin memory for put/get */
int
pin(void* buffer, size_t len)
{
  dbg_assert_precondition(len && buffer);
  dbg_printf("%d: Pinning %zd bytes at %p\n", hpx_get_rank(), len, buffer);

  /* TODO: replace with more specific error */
  return (!photon_register_buffer(buffer, len)) ? HPX_SUCCESS :
    (__hpx_errno = HPX_ERROR);
}

/* unpin memory for put/get */
int
unpin(void* buffer, size_t len)
{
  dbg_assert_precondition(len && buffer);
  dbg_printf("%d: Unpinning/freeing %zd bytes from buffer at %p\n",
             hpx_get_rank(), len, buffer);

/* TODO: replace with more specific error */
  return (!photon_unregister_buffer(buffer, len)) ? HPX_SUCCESS :
    (__hpx_errno = HPX_ERROR);
}

/* Return the physical network ID of the current process */
int
phys_addr(hpx_locality_t *l)
{
  int ret;
  ret = HPX_ERROR;

  if (!l) {
    /* TODO: replace with more specific error */
    __hpx_errno = HPX_ERROR;
    return ret;
  }

  l->rank = rank;
  return 0;
}

/**
 * When Photon is running on ugni, we need to have a message size that's a
 * multiple of four bytes. This performs that adjustment.
 *
 * @todo This isn't necessary for VERBS, so when we're using infiniband we
 *       shouldn't do this.
 */
size_t
get_network_bytes(size_t n)
{
  return ((n + sizeof(uint32_t) - 1) & ~(sizeof(uint32_t) - 1));
}

void
barrier(void)
{
#if HAVE_MPI
  MPI_Barrier(MPI_COMM_WORLD);
#endif
}
