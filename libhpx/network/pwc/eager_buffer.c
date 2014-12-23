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

#include <stddef.h>
#include "libhpx/libhpx.h"
#include "eager_buffer.h"

int eager_buffer_init(eager_buffer_t* b, char *base, uint32_t size) {
  b->size = size;
  b->base = base;
  b->next = 0;
  b->last = 0;
  return LIBHPX_OK;
}


void eager_buffer_fini(eager_buffer_t *b) {
}

void eager_buffer_progress(eager_buffer_t *b) {
}
