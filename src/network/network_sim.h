/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  SIM Network Interface 
  sim.h

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/

#pragma once
#ifndef LIBHPX_NETWORK_SIM_H_
#define LIBHPX_NETWORK_SIM_H_

#include <stdlib.h>

#include "network.h"
#include "hpx/action.h"
#include "hpx/parcel.h"
#include "hpx/runtime.h"

int  sim_init(void);
int  sim_finalize(void);
void sim_progress(void *data);
int  sim_probe(int source, int* flag, network_status_t* status);
int  sim_send(int dest, void *data, size_t len, network_request_t *request);
int  sim_recv(int src, void *buffer, size_t len, network_request_t *request);
int  sim_test(network_request_t *request, int *flag, network_status_t *status);
int  sim_put(int dest, void *buffer, size_t len, network_request_t *request);
int  sim_get(int src, void *buffer, size_t len, network_request_t *request);
int  sim_pin(void* buffer, size_t len);
int  sim_unpin(void* buffer, size_t len);
int  sim_phys_addr(hpx_locality_t *id);

#endif
