// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
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

#ifdef HAVE_JEMALLOC_GLOBAL
# include <jemalloc/jemalloc_global.h>
#else
# include <stdlib.h>
#endif

#include <libhpx/debug.h>
#include <libhpx/memory.h>

#ifdef HAVE_JEMALLOC_GLOBAL
void global_free(void *p) {
  libhpx_global_free(p);
}

void *global_malloc(size_t bytes) {
  return libhpx_global_malloc(bytes);
}

void *global_memalign(size_t boundary, size_t size) {
  return libhpx_global_memalign(boundary, size);
}
#else
void global_free(void *p) {
  free(p);
}

void *global_malloc(size_t bytes) {
  return malloc(bytes);
}

void *global_memalign(size_t boundary, size_t size) {
  void *p;
  int e = posix_memalign(&p, boundary, size);
  dbg_assert(!e);
  return p;
  (void)e;
}
#endif
