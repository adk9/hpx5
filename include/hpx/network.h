/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Network Functions
  hpx_network.h

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).

  Authors:
    Benjamin D. Martin <benjmart [at] indiana.edu>
 ====================================================================
*/

#pragma once
#ifndef LIBHPX_NETWORK_H_
#define LIBHPX_NETWORK_H_

#include <mpi.h>
#if USE_PHOTON
#include "photon.h"
#endif

int hpx_network_send_start(int proc, int tag, char *send_buffer, int size, int offset, uint32_t *send_request);

int hpx_network_recv_start(int proc, int tag, char *recv_buffer, int size, int offset, uint32_t *recv_request);

int hpx_network_wait(uint32_t request);

//int hpx_network_waitany(int *ret_proc,int* ret_req);

int hpx_network_test(uint32_t request, int *flag, int *type, MPI_Status *status);

int hpx_network_init(int* argc, char*** argv);

int hpx_network_rank();

int hpx_network_size();

int hpx_network_finalize();

int hpx_network_register_buffer(char *buffer, int buffer_size);

int hpx_network_unregister_buffer(char *buffer, int buffer_size);

#endif /* LIBHPX_NETWORK_H_ */
