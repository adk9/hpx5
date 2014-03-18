// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#include <stdlib.h>
#include "libhpx/debug.h"
#include "request.h"

request_t *request_init(request_t *request, hpx_parcel_t *p) {
  request->parcel = p;
  return request;
}


request_t *request_new(hpx_parcel_t *p, int request) {
  request_t *r = malloc(sizeof(*r) + request);
  if (!r) {
    dbg_error("could not allocate request.\n");
    return NULL;
  }
  return request_init(r, p);
}

void request_delete(request_t *r) {
  if (!r)
    return;
  free(r);
}
