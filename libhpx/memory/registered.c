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

#ifndef HAVE_JEMALLOC_REGISTERED
# error registered implementation should not be compiled a network
#endif

#include <jemalloc/jemalloc_registered.h>
#include <libhpx/memory.h>

address_space_t *
address_space_new_jemalloc_registered(const struct config *UNUSED) {
  address_space_t *space = malloc(sizeof(*space));
  dbg_assert(space);
  space->delete = free;
  space->free = libhpx_registered_free;
  space->malloc = libhpx_registered_malloc;
  space->calloc = libhpx_registered_calloc;
  space->memalign = libhpx_registered_memalign;
  return space;
}
