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
#ifndef LIBHPX_NETWORK_PWC_SEND_BUFFER_H
#define LIBHPX_NETWORK_PWC_SEND_BUFFER_H

#include <hpx/hpx.h>

struct eager_buffer;

typedef struct {
  struct eager_buffer *endpoint;
} send_buffer_t;

int send_buffer_init(send_buffer_t *sends, struct eager_buffer *endpoint)
  HPX_INTERNAL HPX_NON_NULL(1, 2);

void send_buffer_fini(send_buffer_t *sends)
  HPX_INTERNAL HPX_NON_NULL(1);

int send_buffer_send(send_buffer_t *sends, hpx_parcel_t *p, hpx_addr_t lsync)
  HPX_INTERNAL HPX_NON_NULL(1);

#endif // LIBHPX_NETWORK_PWC_EAGER_BUFFER_H
