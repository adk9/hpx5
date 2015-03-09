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

#ifndef HAVE_JEMALLOC_GLOBAL
# error global implementation should not be compiled a network
#endif

#include <jemalloc/jemalloc_global.h>
#include <libhpx/memory.h>

address_space_t *
address_space_new_jemalloc_global(const struct config *UNUSED) {
  address_space_t *space = malloc(sizeof(*space));
  dbg_assert(space);
  space->delete = free;
  space->free = libhpx_global_free;
  space->malloc = libhpx_global_malloc;
  space->calloc = libhpx_global_calloc;
  space->memalign = libhpx_global_memalign;
  return space;
}
