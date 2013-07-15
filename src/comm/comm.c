/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Communication Layer
  comm.c

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
#include "hpx/comm.h"
#include "hpx/parcel.h"

/* Default communication operations */
comm_operations_t default_operations = {
    .init            = _comm_init,
    .send_parcel     = _comm_send_parcel,
    .send            = _comm_send,
    .put             = _comm_put,
    .get             = _comm_get,
    .progress        = _comm_progress,
    .finalize        = _comm_finalize,
};

int _comm_init(void) {
}

int _comm_send_parcel(hpx_locality_t *, hpx_parcel_t *) {
}

int _comm_send(int peer, void *payload, size_t len) {
}

int _comm_put(int peer, void *dst, void *src, size_t len) {
}

int _comm_get(void *dst, int peer, void *src, size_t len) {
}

void _comm_progress(void *data) {
}

void _comm_finalize(void) {
}
