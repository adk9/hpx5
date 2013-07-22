/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/

#include <mpi.h>   /* TODO: deal with case where there is no mpi */

#pragma once
#ifndef LIBHPX_COMM_H_
#define LIBHPX_COMM_H_

/**
 * Communication Operations
 */

typedef struct comm_operations_t {
  /* Initialize the communication layer */
  int (*init)(void);
  /* Helper to send a parcel structure */
  int (*send_parcel)(hpx_locality_t *, hpx_parcel_t *);
  /* Send a raw payload */
  int (*send)(int dest, void *buffer, size_t len, comm_request_t);
  /* Receive raw data */
  int (*recv)(void *buffer, comm_request_t);
  /* test for completion of send or receive */
  int (*sendrecv_test)(comm_request_t *request, int *flag, comm_status_t *status);
  /* RDMA put */
  int (*put)(void *buffer, size_t len, comm_request_t *request);
  /* RDMA get */
  int (*get)(void *buffer, size_t len);
  /* test for completion of put or get */
  int (*putget_test)(comm_request_t *request, int *flag, comm_status_t *status);
  /* Shutdown and clean up the communication layer */
  void (*finalize)(void);
} comm_operations_t;

typedef struct comm_request_t {
  /* TODO: deal with case where there is no mpi */
  MPI_Request mpi;
  uint32_t photon;
} comm_request_t;

typedef struct comm_status_t {
  /* TODO: deal with case where there is no mpi */
  MPI_Status mpi;
  MPI_Status photon; /* not a mistake - Photon uses MPI status */
}

/**
 * Default communication operations
 */

/* Initialize the communication layer */
int _comm_init(void);

/* Helper to send a parcel structure */
int _comm_send_parcel(hpx_locality_t *, hpx_parcel_t *);

/* Send a raw payload */
int _comm_send(int peer, void *payload, size_t len);

/* RMA put */
int _comm_put(int peer, void *dst, void *src, size_t len);

/* RMA get */
int _comm_get(void *dst, int peer, void *src, size_t len);

/* The communication progress function */
void _comm_progress(void *data);

/* Shutdown and clean up the communication layer */
void _comm_finalize(void);

#endif /* LIBHPX_COMM_H_ */
