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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "libhpx/debug.h"
#include "libhpx/libhpx.h"
#include "eager_buffer.h"
#include "send_buffer.h"

int send_buffer_init(send_buffer_t *sends, struct eager_buffer *endpoint) {
  sends->endpoint = endpoint;
  return LIBHPX_OK;
}

void send_buffer_fini(send_buffer_t *sends) {
  dbg_error("unimplemented\n");
}

int send_buffer_send(send_buffer_t *sends, hpx_parcel_t *p, hpx_addr_t lsync) {
  return LIBHPX_EUNIMPLEMENTED;
}
