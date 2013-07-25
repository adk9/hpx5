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

#include <stdlib.h>

#include "hpx/action.h"
#include "hpx/network.h"
#include "hpx/parcel.h"
#include "hpx/network/mpi.h"
#include "hpx/network/photon.h"

#include <mpi.h>
#include <photon.h>


/* Photon network operations */
network_ops_t photon_ops = {
  .init     = _init_photon,
  .finalize = _finalize_photon,
  .progress = _progress_photon,
  .send     = _send_mpi,
  .recv     = _recv_mpi,
  .sendrecv_test = _test_mpi,
  .put      = _put_photon,
  .get      = _get_photon,
  .putget_test = _test_photon,
  .pin      = _pin_photon,
  .unpin    = _unpin_photon,
};

int _eager_threshold_PHOTON = _EAGER_THRESHOLD_PHOTON_DEFAULT;

/* If using Photon, call this instead of _init_mpi */
int _init_photon(void) {
  int retval;
  int temp;
  int thread_support_provided;
  int rank;
  int size;

  retval = HPX_ERROR;

  /* TODO: see if we really need thread multiple */
  temp = MPI_Init_thread(0, NULL, MPI_THREAD_MULTIPLE, &thread_support_provided); /* TODO: should be argc and argv if possible */
  if (temp == MPI_SUCCESS)
    retval = 0;
  else {
    __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */
    goto error;
  }

  MPI_Comm_rank(MPI_COMM_WORLD, &rank); /* TODO: cache */
  MPI_Comm_size(MPI_COMM_WORLD, &size); /* TODO: cache */
  temp =  photon_init(size, rank, MPI_COMM_WORLD);
  if (temp == 0)
    retval = 0;
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


/* have to test/wait on the request */
int _put_photon(int dest, void* buffer, size_t len, network_request_t *request) {
  int temp;
  int retval;
  int rank;
  int tag;

  retval = HPX_ERROR;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank); /* TODO: cache this */
  tag = rank;

  if (len > UINT32_MAX) {
    __hpx_errno = HPX_ERROR;
    retval = HPX_ERROR;
    goto error;
  }

  temp = photon_post_recv_buffer_rdma(dest, buffer, (uint32_t)len, tag, &(request->photon));
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
  int rank;
  int tag;

  retval = HPX_ERROR;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank); /* TODO: cache this */
  tag = rank;

  if (len > UINT32_MAX) {
    __hpx_errno = HPX_ERROR;
    retval = HPX_ERROR;    
    goto error;
  }

  temp = photon_wait_recv_buffer_rdma(src, tag);
  if (temp != 0) {
    __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */
    goto error;
  }
  temp = photon_post_os_put(src, buffer, (uint32_t)len, tag, 0, &(request->photon));
  if (temp != 0) {
    __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */
    goto error;
  }
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
  retval = HPX_ERROR;

  int type; /* I'm not actually sure what this does with photon. 0 is event, 1 is ledger but I don't know why I care */

  if (status == NULL)
    temp = photon_test((request->photon), flag, &type, MPI_STATUS_IGNORE);
  else
    temp = photon_test((request->photon), flag, &type, &(status->photon));

  if (temp == 0)
    retval = 0;
  else
    __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */

  return retval;  
}

int _send_parcel_photon(hpx_locality_t *loc, hpx_parcel_t *parc) {
}

int _send_photon(int peer, void *payload, size_t len, network_request_t *request) {
}

int _recv_photon(int src, void *buffer, size_t len, network_request_t *request) {
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

  temp = photon_unregister_buffer(buffer, len);
  if (temp != 0) {
    __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */
    goto error;
  }

 error:
  return retval;
}
