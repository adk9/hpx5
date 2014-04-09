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
#ifndef LIBHPX_TRANSPORT_PROGRESS_H
#define LIBHPX_TRANSPORT_PROGRESS_H

#include "hpx/hpx.h"

typedef struct request request_t;
struct request {
  request_t    *next;
  hpx_parcel_t *parcel;
  char          request[];
};

typedef struct progress progress_t;
struct progress {
  const boot_t      *boot;
  transport_t       *transport;
  request_t         *free;              // request freelist
  request_t         *pending_sends;     // outstanding send requests
  request_t         *pending_recvs;     // outstanding recv requests
};

HPX_INTERNAL progress_t *network_progress_new(const boot_t *boot, transport_t *transport) HPX_NON_NULL(1) HPX_MALLOC;
HPX_INTERNAL void network_progress_poll(progress_t *p) HPX_NON_NULL(1);
HPX_INTERNAL void network_progress_flush(progress_t *p) HPX_NON_NULL(1);
HPX_INTERNAL void network_progress_delete(progress_t *p) HPX_NON_NULL(1);

#endif // LIBHPX_TRANSPORT_PROGRESS_H
