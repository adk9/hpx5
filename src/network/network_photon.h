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

#pragma once
#ifndef LIBHPX_NETWORK_PHOTON_H_
#define LIBHPX_NETWORK_PHOTON_H_

#include <stdlib.h>
#include <mpi.h>
#include <photon.h>

#include "hpx/action.h"
#include "hpx/parcel.h"

#include "network/network.h"

#include "network_mpi.h"

/*
 * Forward declarations - things in network.h we need here
 */
struct network_ops_t;
struct network_request_t;
struct network_status_t;

#define EAGER_THRESHOLD_PHOTON_DEFAULT 256

/* If using Photon, call this instead of _init_mpi */
int init_photon(void);

/* If you're using Photon, call this instead of _finalize_mpi */
int finalize_photon(void);

void progress_photon(void *data);

int probe_photon(int source, int* flag, network_status_t* status);

/* have to test/wait on the request */
int put_photon(int dest, void* buffer, size_t len, network_request_t *request);

int get_photon(int src, void* buffer, size_t len, network_request_t *request);

int test_photon(network_request_t *request, int *flag, network_status_t *status);

int test_get_photon(network_request_t *request, int *flag, network_status_t *status);

int send_parcel_photon(hpx_locality_t *, hpx_parcel_t *);

int send_photon(int peer, void *payload, size_t len, network_request_t *request);

int recv_photon(int src, void *buffer, size_t len, network_request_t *request);

/* pin memory for put/get */
int pin_photon(void* buffer, size_t len);

/* unpin memory for put/get */
int unpin_photon(void* buffer, size_t len);

int phys_addr_photon(hpx_locality_t *l);

#endif
