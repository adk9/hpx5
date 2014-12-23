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
#ifndef LIBHPX_NETWORK_PWC_SEGMENT_H
#define LIBHPX_NETWORK_PWC_SEGMENT_H

#include <hpx/hpx.h>
#include <photon.h>


typedef struct segment {
  char                      *base;
  size_t                     size;
  struct photon_buffer_priv_t key;
} segment_t;

///
int segment_init(segment_t *segment, char *base, size_t size)
  HPX_INTERNAL HPX_NON_NULL(1, 2);

void segment_fini(segment_t *segment)
  HPX_INTERNAL HPX_NON_NULL(1);

static inline void *segment_offset_to_rva(segment_t *segment, size_t offset) {
  assert(offset < segment->size);
  return segment->base + offset;
}

#endif
