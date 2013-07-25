/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  MPI Network Interface 
  mpi.c

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/

#include <stdlib.h>
#include <mpi.h>

#include "hpx/action.h"
#include "hpx/network.h"
#include "hpx/parcel.h"
#include "hpx/network/mpi.h"

/* MPI network operations */
network_ops_t mpi_ops = {
    .init     = _init_mpi,
    .finalize = _finalize_mpi,
    .progress = _progress_mpi,
    .send     = _send_mpi,
    .recv     = _recv_mpi,
    .sendrecv_test = _test_mpi,
    .put      = _put_mpi,
    .get      = _get_mpi,
    .putget_test = _test_mpi,
};

int _eager_threshold_mpi = _EAGER_THRESHOLD_MPI_DEFAULT;

int _init_mpi(void) {
  int retval;
  int temp;
  int thread_support_provided;

  retval = HPX_ERROR;

  /* TODO: see if we really need thread multiple */
  temp = MPI_Init_thread(0, NULL, MPI_THREAD_MULTIPLE, &thread_support_provided); /* TODO: should be argc and argv if possible */
  if (temp == MPI_SUCCESS)
    retval = 0;
  else
    __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */
  
  return retval;
}

int _send_parcel_mpi(hpx_locality_t * loc, hpx_parcel_t * parc) {
  /* pseudocode:
     if size > eager_threshold:
       send notice to other process of intent to put via rdma
       put data via rdma
     else:
       send parcel using _send_mpi
  */
}

/* Send data via MPI. Presumably this will be an "eager" send. Don't use "data" until it's done! */
int _send_mpi(int dest, void *data, size_t len, network_request_t *request) {
  int retval;
  int temp;
  int rank;

  retval = HPX_ERROR;

  /*
  if (len > INT_MAX) {
    __hpx_errno = HPX_ERROR;
    retval = HPX_ERROR;
  }
  */ /* not necessary because of eager_threshold */
  if (len > _eager_threshold_mpi) { /* need to use _network_put_* for that */
    __hpx_errno = HPX_ERROR;
    retval = HPX_ERROR;    
  }

  MPI_Comm_rank(MPI_COMM_WORLD, &rank); /* TODO: cache this, obviously */
  temp = MPI_Isend(data, (int)len, MPI_BYTE, dest, rank, MPI_COMM_WORLD, &(request->mpi));

  if (temp == MPI_SUCCESS)
    retval = 0;
  else
    __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */

  return retval;  
}

/* this is non-blocking recv - user must test/wait on the request */
int _recv_mpi(int source, void* buffer, size_t len, network_request_t *request) {
  int retval;
  int temp;
  int mpi_src;
  int mpi_len;

  retval = HPX_ERROR;
  if (source == NETWORK_ANY_SOURCE)
    mpi_src = MPI_ANY_SOURCE;
  if (len == NETWORK_ANY_LENGTH)
    mpi_len = _eager_threshold_mpi;
  else {
    if (len > _eager_threshold_mpi) { /* need to use _network_put_* for that */
      __hpx_errno = HPX_ERROR;
      retval = HPX_ERROR;    
      goto error;
    }
  }

  temp = MPI_Irecv(buffer, (int)mpi_len, MPI_BYTE, mpi_src, MPI_ANY_TAG, MPI_COMM_WORLD, &(request->mpi));

  if (temp == MPI_SUCCESS)
    retval = 0;
  else
    __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */

 error:
  return retval;  
}

/* status may be NULL */
int _test_mpi(network_request_t *request, int *flag, network_status_t *status) {
  int retval;
  int temp;
  retval = HPX_ERROR;

  if (status == NULL)
    temp = MPI_Test(&(request->mpi), flag, MPI_STATUS_IGNORE);
  else
    temp = MPI_Test(&(request->mpi), flag, &(status->mpi));

  if (temp == MPI_SUCCESS)
    retval = 0;
  else
    __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */

  return retval;  
}

int _put_mpi(int dest, void *buffer, size_t len, network_request_t *request) {
}

int _get_mpi(int src, void *buffer, size_t len, network_request_t *request) {
}

void _progress_mpi(void *data) {
}

int _finalize_mpi(void) {
  int retval;
  int temp;
  retval = HPX_ERROR;

  temp = MPI_Finalize();

  if (temp == MPI_SUCCESS)
    retval = 0;
  else
    __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */

  return retval;
}

