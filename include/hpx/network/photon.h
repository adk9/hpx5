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
#include <photon.h>

#include "hpx/action.h"
#include "hpx/network.h"
#include "hpx/parcel.h"

extern int _network_eager_threshold = 256;

/* If using Photon, call this instead of _init_mpi */
int _init_photon(void);

/* If you're using Photon, call this instead of _finalize_mpi */
int _finalize_photon(void);

void _progress_photon(void *data);

/* have to test/wait on the request */
int _put_photon(void* buffer, size_t len, comm_request_t *request);

int _get_photon(void* buffer, size_t len);

int _test_photon(comm_request_t *request, int *flag, comm_status_t *status);

int _send_parcel_photon(hpx_locality_t *, hpx_parcel_t *);

int _send_photon(int peer, void *payload, size_t len);

int _recv_photon(void *buffer, comm_request_t req);

/* Photon communication operations */
network_ops_t photon_ops = {
  .init     = _init_photon,
  .finalize = _finalize_photon,
  .progress = _progress_photon,
  .send     = _send_mpi,
  .recv     = _recv_mpi,
  .sendrecv_test = _test_mpi
  .put      = _put_photon,
  .get      = _get_photon,
  .putget_test = _test_photon
  .pin      = _pin_photon,
  .unpin    = _unpin_photon,
};
