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
#ifndef LIBHPX_NETWORK_PWC_CIRCULAR_BUFFER_H
#define LIBHPX_NETWORK_PWC_CIRCULAR_BUFFER_H

#include <stdint.h>
#include <hpx/attributes.h>

typedef struct {
  uint32_t  size;
  uint32_t esize;
  uint64_t   max;
  uint64_t   min;
  void  *records;
} circular_buffer_t;

int circular_buffer_init(circular_buffer_t *b, uint32_t esize, uint32_t size)
  HPX_INTERNAL HPX_NON_NULL(1);

void circular_buffer_fini(circular_buffer_t *b)
  HPX_INTERNAL HPX_NON_NULL(1);

void *circular_buffer_append(circular_buffer_t *b)
  HPX_INTERNAL HPX_NON_NULL(1);

int circular_buffer_progress(circular_buffer_t *b,
                             int (*progress_callback)(void *env, void *record),
                             void *progress_env)
  HPX_INTERNAL HPX_NON_NULL(1, 2);

#endif // LIBHPX_NETWORK_PWC_CIRCULAR_BUFFER_H
