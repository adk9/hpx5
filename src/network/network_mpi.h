/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  MPI Network Interface 
  mpi.h

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/

#pragma once
#ifndef LIBHPX_NETWORK_MPI_H_
#define LIBHPX_NETWORK_MPI_H_

#include <limits.h>
#include <stdlib.h>
#include <mpi.h>

#include "network.h"
#include "hpx/action.h"
#include "hpx/parcel.h"
#include "hpx/runtime.h"


//#define EAGER_THRESHOLD_MPI_DEFAULT 10240
#define EAGER_THRESHOLD_MPI_DEFAULT INT_MAX
/* TODO: make reasonable once we have puts/gets working */

int init_mpi(void);
int finalize_mpi(void);
void progress_mpi(void *data);
int probe_mpi(int source, int* flag, network_status_t* status);
int send_parcel_mpi(hpx_locality_t *, hpx_parcel_t *);

/* Send data via MPI. Presumably this will be an "eager" send. Don't use "data" until it's done! */
int send_mpi(int dest, void *data, size_t len, network_request_t *request);

/**
   this is non-blocking recv - user must test/wait on the request
 */
int recv_mpi(int src, void *buffer, size_t len, network_request_t *request);

/* status may be NULL */
int test_mpi(network_request_t *request, int *flag, network_status_t *status);
int put_mpi(int dest, void *buffer, size_t len, network_request_t *request);
int get_mpi(int src, void *buffer, size_t len, network_request_t *request);
int pin_mpi(void* buffer, size_t len);
int unpin_mpi(void* buffer, size_t len);
int phys_addr_mpi(hpx_locality_t *id);

#endif
