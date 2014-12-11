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
#ifndef LIBHPX_NETWORK_PWC_BUFFER_H
#define LIBHPX_NETWORK_PWC_BUFFER_H

#include <hpx/hpx.h>

struct segment;
struct photon_buffer_priv_t;
struct pwc_record;

typedef struct {
  uint32_t              rank;
  uint32_t              size;
  uint64_t               min;
  uint64_t               max;
  struct pwc_record *records;
} pwc_buffer_t;


int pwc_buffer_init(pwc_buffer_t *buffer, uint32_t rank, uint32_t size)
  HPX_INTERNAL HPX_NON_NULL(1);


void pwc_buffer_fini(pwc_buffer_t *buffer)
  HPX_INTERNAL HPX_NON_NULL(1);


int pwc_buffer_put(pwc_buffer_t *buffer, size_t offset, const void *lva, size_t n,
                   hpx_addr_t local, hpx_addr_t remote, hpx_action_t op,
                   struct segment *segment)
  HPX_INTERNAL HPX_NON_NULL(1);


int pwc_buffer_progress(pwc_buffer_t *buffer)
  HPX_INTERNAL HPX_NON_NULL(1);


#endif // LIBHPX_NETWORK_PWC_BUFFER_H
