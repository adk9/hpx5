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

#include "libhpx/libhpx.h"
#include "pgas.h"
#include "pgas_heap.h"

static lhpx_pgas_heap_t _heap = {
  .csbrk = 0,
  .bytes_per_chunk = 0,
  .nchunks = 0,
  .chunks = NULL,
  .nbytes = 0,
  .bytes = NULL
};

int lhpx_pgas_init(size_t heap_size) {
  return lhpx_pgas_heap_init(&_heap, heap_size);
}

int lhpx_pgas_init_worker() {
  return LIBHPX_EUNIMPLEMENTED;
}

void lhpx_pgas_fini_worker() {
}

void lhpx_pgas_fini(void) {
  lhpx_pgas_heap_fini(&_heap);
}
