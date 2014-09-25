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

#include <stdbool.h>
#include <jemalloc/jemalloc.h>
#include "libhpx/debug.h"
#include "jemalloc_mallctl_wrappers.h"

size_t lhpx_jemalloc_get_chunk_size(void) {
  size_t log2_bytes_per_chunk = 0;
  size_t sz = sizeof(log2_bytes_per_chunk);
  int e = mallctl("opt.lg_chunk", &log2_bytes_per_chunk, &sz, NULL, 0);
  if (e) {
    dbg_error("pgas: failed to read the jemalloc chunk size\n");
  }

  return 1 << log2_bytes_per_chunk;
}
