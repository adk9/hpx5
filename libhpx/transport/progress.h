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

struct transport;

typedef struct request request_t;
struct request {
  request_t      *next;
  hpx_parcel_t *parcel;
  char       request[];
};

typedef struct progress progress_t;
struct progress {
  uint32_t         psend_limit;       //
  uint32_t             npsends;       //
  request_t     *pending_sends;       // outstanding send requests
  uint32_t         precv_limit;       //
  uint32_t             nprecvs;       //
  request_t     *pending_recvs;       // outstanding recv requests
  hpx_parcel_t          *recvs;       // completed recvs
};


progress_t *network_progress_new(struct transport *transport)
  HPX_INTERNAL HPX_MALLOC HPX_NON_NULL(1);


void network_progress_poll(progress_t *p)
  HPX_INTERNAL HPX_NON_NULL(1);


void network_progress_flush(progress_t *p)
  HPX_INTERNAL HPX_NON_NULL(1);


void network_progress_delete(progress_t *p)
  HPX_INTERNAL HPX_NON_NULL(1);

static inline int network_progress_can_send(progress_t *p) {
  return (p->npsends < p->psend_limit);
}

static inline int network_progress_can_recv(progress_t *p) {
  return (p->nprecvs < p->precv_limit);
}

static inline int network_progress_drain_sends(progress_t *p) {
  return !network_progress_can_send(p);
}

static inline int network_progress_drain_recvs(progress_t *p) {
  return !network_progress_can_recv(p);
}

#endif // LIBHPX_TRANSPORT_PROGRESS_H
