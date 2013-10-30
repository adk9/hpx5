/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Network Layer
  network.c

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdbool.h>

#include "network.h"
#include "hpx/error.h"

static int init(void);
static int finalize(void);
static int probe(int source, int* flag, network_status_t* status);
static int send(int dest, void *buffer, size_t len, network_request_t* req);
static int recv(int src, void *buffer, size_t len, network_request_t* req);
static int put(int dest, void *buffer, size_t len, network_request_t* req);
static int get(int src, void *buffer, size_t len, network_request_t* req);
static int test(network_request_t *request, int *flag, network_status_t *status);
static int pin(void* buffer, size_t len);
static int unpin(void* buffer, size_t len);
static int phys_addr(hpx_locality_t *l);
static void progress(void *data);

/* Default network operations */
network_ops_t default_net_ops = {
    .init     = init,
    .finalize = finalize,
    .progress = progress,
    .probe    = probe,
    .send     = send,
    .recv     = recv,
    .sendrecv_test     = test,
    .put      = put,
    .get      = get,
    .putget_test     = test,
    .pin      = pin,
    .unpin    = unpin,
    .phys_addr= phys_addr,
};

/*
 * Stub versions
 */

int init(void) {
  return 0;
}

int finalize(void) {
  return 0;
}

void progress(void *data) {
}

int probe(int source, int* flag, network_status_t* status) {
  *flag = false;
  return 0;
}

int send(int dest, void *buffer, size_t len, network_request_t* req) {
  __hpx_errno = HPX_ERROR;
  return HPX_ERROR;
}

int recv(int src, void *buffer, size_t len, network_request_t* req) {
  __hpx_errno = HPX_ERROR;
  return HPX_ERROR;
}

int put(int dest, void *buffer, size_t len, network_request_t* req) {
  __hpx_errno = HPX_ERROR;
  return HPX_ERROR;
}

int get(int src, void *buffer, size_t len, network_request_t* req) {
  __hpx_errno = HPX_ERROR;
  return HPX_ERROR;
}

int test(network_request_t *request, int *flag, network_status_t *status) {
  __hpx_errno = HPX_ERROR;
  return HPX_ERROR;
}  

int pin(void* buffer, size_t len) {
  return 0;
}

int unpin(void* buffer, size_t len) {
  return 0;
}

int phys_addr(hpx_locality_t *l) {
  return 0;
}

/**********************************************************/

void hpx_network_barrier() {
#if HAVE_MPI
  MPI_Barrier(MPI_COMM_WORLD);
#endif
}
