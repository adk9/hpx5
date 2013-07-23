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

int _network_eager_threshold = 256;

/* MPI communication operations */
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

int _init_mpi(void) {
  int retval;
  int temp;
  int thread_support_provided;

  retval = -1;

  /* TODO: see if we really need thread multiple */
  temp = MPI_Init_thread(0, NULL, MPI_THREAD_MULTIPLE, &thread_support_provided); /* TODO: should be argc and argv if possible */
  if (temp == MPI_SUCCESS)
    retval = 0;
  else
    __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */
  
  return retval;
}

int _send_parcel_mpi(hpx_locality_t *, hpx_parcel_t *) {
  /* pseudocode:
     if size > eager_threshold:
       send notice to other process of intent to put via rdma
       put data via rdma
     else:
       send parcel using _send_mpi
  */
}

/* Send data via MPI. Presumably this will be an "eager" send. Don't use "data" until it's done! */
int _send_mpi(int dest, void *data, size_t len, comm_request_t *request) {
  int temp;
  int rank;

  /*
  if (len > INT_MAX) {
    __hpx_errno = HPX_ERROR;
    retval = -1;
  }
  */ /* not necessary because of eager_threshold */
  if (len > _comm_eager_threshold) { /* need to use _comm_put_* for that */
    __hpx_errno = HPX_ERROR;
    retval = -1;    
  }

  MPI_rank(&rank); /* TODO: cache this, obviously */
  temp = MPI_Isend(data, (int)len, MPI_BYTE, dest, -1, rank, MPI_COMM_WORLD, request->mpi);

  if (temp == MPI_SUCCESS)
    retval = 0;
  else
    __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */

  return retval;  
}

/* this is non-blocking recv - user must test/wait on the request */
int _recv_mpi(void* buffer, comm_request_t *request) {
  int retval;
  int temp;
  retval = -1;

  temp = MPI_Irecv(buffer, _comm_eager_threshold, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, request.mpi);

  if (temp == MPI_SUCCESS)
    retval = 0;
  else
    __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */

  return retval;  
}

/* status may be NULL */
int _test_mpi(comm_request_t *request, int *flag, comm_status_t *status) {
  int retval;
  int temp;
  retval = -1;

  if (status == NULL)
    temp = MPI_Test(request, &flag, MPI_STATUS_IGNORE);
  else
    temp = MPI_Test(request, &flag, status.mpi);

  if (temp == MPI_SUCCESS)
    retval = 0;
  else
    __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */

  return retval;  
}

int _put_mpi(int peer, void *dst, void *src, size_t len) {
}

int _get_mpi(void *dst, int peer, void *src, size_t len) {
}

void _progress_mpi(void *data) {
}

int _finalize_mpi(void) {
  int retval;
  int temp;
  retval = -1;

  temp = MPI_Finalize();

  if (temp == MPI_SUCCESS)
    retval = 0;
  else
    __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */

  return retval;
}

