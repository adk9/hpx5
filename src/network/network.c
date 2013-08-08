/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Networkunication Layer
  network.c

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
#include "hpx/network.h"
#include "hpx/parcel.h"
#include "hpx/runtime.h" /* for hpx_locality_t */

/* Default networkunication operations */
network_ops_t default_ops = {
    .init     = hpx_network_init,
    .finalize = hpx_network_finalize,
    .progress = hpx_network_progress,
    .probe    = hpx_network_probe,
    .send     = hpx_network_send,
    .recv     = hpx_network_recv,
    .put      = hpx_network_put,
    .get      = hpx_network_get,
    .pin      = hpx_network_pin,
    .unpin      = hpx_network_unpin,
};

/*
 * Stub versions
 */

int hpx_network_init(void) {
  return 0;
}

int hpx_network_finalize(void) {
  return 0;
}

void hpx_network_progress(void *data) {
}

int hpx_network_probe(int source, int* flag, network_status_t* status) {
  *flag = false;
  return 0;
}

int hpx_network_send(int dest, void *buffer, size_t len, network_request_t* req) {
  __hpx_errno = HPX_ERROR;
  return HPX_ERROR;
}

int hpx_network_recv(int src, void *buffer, size_t len, network_request_t* req) {
  __hpx_errno = HPX_ERROR;
  return HPX_ERROR;
}

int hpx_sendrecv_test(network_request_t *request, int *flag, network_status_t *status) {
  __hpx_errno = HPX_ERROR;
  return HPX_ERROR;
}  

int hpx_network_put(int dest, void *buffer, size_t len, network_request_t* req) {
  __hpx_errno = HPX_ERROR;
  return HPX_ERROR;
}

int hpx_network_get(int src, void *buffer, size_t len, network_request_t* req) {
  __hpx_errno = HPX_ERROR;
  return HPX_ERROR;
}

int hpx_putget_test(network_request_t *request, int *flag, network_status_t *status) {
  __hpx_errno = HPX_ERROR;
  return HPX_ERROR;
}  

int hpx_network_pin(void* buffer, size_t len) {
  return 0;
}

int hpx_network_unpin(void* buffer, size_t len) {
  return 0;
}
