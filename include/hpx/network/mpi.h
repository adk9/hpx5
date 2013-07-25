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

#include <stdlib.h>
#include <mpi.h>

#include "hpx/action.h"
#include "hpx/network.h"
#include "hpx/parcel.h"

#define _EAGER_THRESHOLD_MPI_DEFAULT 256;
extern int _eager_threshold_mpi;

int _init_mpi(void);

int _finalize_mpi(void);

void _progress_mpi(void *data);

int _send_parcel_mpi(hpx_locality_t *, hpx_parcel_t *);

/* Send data via MPI. Presumably this will be an "eager" send. Don't use "data" until it's done! */
int _send_mpi(int dest, void *data, size_t len, network_request_t *request);

/**
   this is non-blocking recv - user must test/wait on the request
 */
int _recv_mpi(int src, void *buffer, size_t len, network_request_t *request);

/* status may be NULL */
int _test_mpi(network_request_t *request, int *flag, network_status_t *status);

int _put_mpi(int dest, void *buffer, size_t len, network_request_t *request);

int _get_mpi(int src, void *buffer, size_t len, network_request_t *request);

extern network_ops_t mpi_ops;

#endif
