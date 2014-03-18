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
#ifndef LIBHPX_NETWORK_REQUEST_H
#define LIBHPX_NETWORK_REQUEST_H

#include <hpx.h>

typedef struct request request_t;
struct request {
  request_t      *next;
  hpx_parcel_t *parcel;
  char       request[];
};

HPX_INTERNAL request_t *request_new(hpx_parcel_t *p, int request) HPX_NON_NULL(1) HPX_MALLOC;
HPX_INTERNAL request_t *request_init(request_t *r, hpx_parcel_t *p) HPX_NON_NULL(1, 2);
HPX_INTERNAL void request_delete(request_t *r) HPX_NON_NULL(1);

#endif // LIBHPX_NETWORK_REQUEST_H
