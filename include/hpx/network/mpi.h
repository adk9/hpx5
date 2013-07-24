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

#include <stdlib.h>
#include <mpi.h>

#include "hpx/action.h"
#include "hpx/network.h"
#include "hpx/parcel.h"

extern int _eager_threshold_mpi = 256;

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

int _put_mpi(int peer, void *dst, void *src, size_t len);

int _get_mpi(void *dst, int peer, void *src, size_t len);

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



