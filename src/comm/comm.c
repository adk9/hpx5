/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Communication Layer
  comm.c

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/

#include <limits.h>
#include <stdlib.h>

#include "hpx/action.h"
#include "hpx/comm.h"
#include "hpx/parcel.h"
#include "hpx/runtime.h" /* for hpx_locality_t */

#include <mpi.h>

int _comm_eage_threshold = 256; /* is big better or worse? smaller means less likelihood of memory problems but higher latency */

/* Default communication operations */
comm_operations_t default_operations = {
    .init            = _comm_init,
    .send_parcel     = _comm_send_parcel,
    .send            = _comm_send,
    .recv            = _comm_recv,
    .sendrecv_test   = _comm_sendrecv_test,
    .put             = _comm_put,
    .get             = _comm_get,
    .putget_test     = _comm_putget_test,
    .finalize        = _comm_finalize,
    .eager_threshold = _comm_edge_threshold,
};

/*
 * Stub versions
 */

int _comm_init(void) {}

void _comm_finalize(void) {}

int _comm_send_parcel(hpx_locality_t *, hpx_parcel_t *) {}

int _comm_send(int dest, void *buffer, size_t len, comm_request_t) {}

int _comm_recv(void *buffer, comm_request_t) {}

int _comm_sendrecv_test(comm_request_t *request, int *flag, comm_status_t *status) {}

int _comm_put(void *buffer, size_t len, comm_request_t *request) {}

int _comm_get(void *buffer, size_t len) {}

int _comm_putget_test(comm_request_t *request, int *flag, comm_status_t *status) {}


/*
 * MPI variants
 */

/* If you're using Photon, call _comm_init_photon instead (or else)! */
int _comm_init_mpi(void) {
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

/* If you're using Photon, call _comm_finalize_photon instead (or else)! */
int _comm_finalize_mpi(void) {
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

int _comm_send_parcel_mpi(hpx_locality_t *, hpx_parcel_t *) {
  /* pseudocode:
     if size > eager_threshold:
       send notice to other process of intent to put via rdma
       put data via rdma
     else:
       send parcel using _comm_send_mpi
  */
}

/* Send data via MPI. Presumably this will be an "eager" send. Don't use "data" until it's done! */
int _comm_send_mpi(int dest, void *data, size_t len, comm_request_t *request) {
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
int _comm_recv_mpi(void* buffer, comm_request_t *request) {
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
int _comm_sendrecv_test_mpi(comm_request_t *request, int *flag, comm_status_t *status) {
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

/*
 * Photon functions
 */

/* If using Photon, call this instead of _comm_init_mpi */
int _comm_init_photon(void) {
  int retval;
  int temp;
  int thread_support_provided;
  int rank;
  int size;

  retval = -1;

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

/* If you're using Photon, call this instead of _comm_finalize_mpi */
int _comm_finalize_photon(void) {
  int retval;
  int temp;
  retval = -1;

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
int _comm_put_photon(void* buffer, size_t len, comm_request_t *request) {
  int rank;
  MPI_rank(&rank); /* TODO: cache this */

  if (len > UINT32_MAX)
    __hpx_errno = HPX_ERROR;
    retval = -1;    
  }

  photon_post_recv_buffer_rdma(rank, buffer, (uint32_t)len, rank, request.photon);
  return 0;
}

int _comm_get_photon(void* buffer, size_t len) {
  int rank;
  MPI_rank(&rank); /* TODO: cache this */

  if (len > UINT32_MAX)
    __hpx_errno = HPX_ERROR;
    retval = -1;    
  }

  photon_wait_recv_buffer_rdma(rank, rank);
  photon_post_os_put(rank, buffer, (uint32_t)len, rank, 0, request.photon);
  photon_send_FIN(rank);
  return 0;
}

int _comm_putget_test_photon(comm_request_t *request, int *flag, comm_status_t *status) {
  int retval;
  int temp;
  retval = -1;

  int type; /* I'm not actually sure what this does with photon. 0 is event, 1 is ledger but I don't know why I care */

  if (status == NULL)
    temp = photon_test(request, flag, &type, MPI_STATUS_IGNORE);
  else
    temp = photon_test(request, flag, &type, status.photon);

  if (temp == 0)
    retval = 0;
  else
    __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */

  return retval;  
}
