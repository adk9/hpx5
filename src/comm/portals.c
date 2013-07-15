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
#include <portals4.h>

#include "hpx/action.h"
#include "hpx/comm.h"
#include "hpx/parcel.h"

/* Portals communication operations */
comm_operations_t portals_operations = {
    .init            = portals_init,
    .send_parcel     = portals_send_parcel,
    .send            = portals_send,
    .put             = portals_put,
    .get             = portals_get,
    .progress        = portals_progress,
    .finalize        = portals_finalize,
};

int portals_init(void) {
}

int portals_send_parcel(hpx_locality_t *, hpx_parcel_t *) {
}

int portals_send(int peer, void *payload, size_t len) {
}

int portals_put(int peer, void *dst, void *src, size_t len) {
}

int portals_get(void *dst, int peer, void *src, size_t len) {
}

void portals_progress(void *data) {
}

void portals_finalize(void) {
}

