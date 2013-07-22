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

#include <stdlib.h>

#include "hpx/action.h"
#include "hpx/network.h"
#include "hpx/parcel.h"

/* Default networkunication operations */
network_ops_t default_ops = {
    .init     = hpx_network_init,
    .send     = hpx_network_send,
    .recv     = hpx_network_recv,
    .put      = hpx_network_put,
    .get      = hpx_network_get,
    .progress = hpx_network_progress,
    .finalize = hpx_network_finalize,
};

int hpx_network_init(void) {
}

int hpx_network_send(int peer, void *src, size_t len) {
}

int hpx_network_recv(int peer, void *dst, size_t len) {
}

int hpx_network_put(int peer, void *dst, void *src, size_t len) {
}

int hpx_network_get(void *dst, int peer, void *src, size_t len) {
}

void hpx_network_progress(void *data) {
}

void hpx_network_finalize(void) {
}
