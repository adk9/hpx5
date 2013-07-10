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


int comm_init(void) {
}

int comm_send_parcel(hpx_locality_t *, hpx_parcel_t *) {
}

int comm_send(int peer, void *payload, size_t len) {
}

int comm_put(int peer, void *dst, void *src, size_t len) {
}

int comm_get(void *dst, int peer, void *src, size_t len) {
}

void comm_progress_engine(void *data) {
}

void comm_finalize(void) {
}
