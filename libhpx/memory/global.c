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

#ifndef HAVE_NETWORK
# error global implementation should not be compiled without a network
#endif

#ifndef HAVE_JEMALLOC_GLOBAL
# error global implementation should not be compiled without jemalloc support
#endif

#include <jemalloc/jemalloc_global.h>
#include <libhpx/memory.h>

static address_space_t _global_address_space_vtable;

static void _global_delete(void *space) {
  dbg_assert(space == &_global_address_space_vtable);
}

static void _global_join(void *space) {
  dbg_assert(space == &_global_address_space_vtable);
}

static void _global_leave(void *space) {
  dbg_assert(space == &_global_address_space_vtable);
}

static address_space_t _global_address_space_vtable = {
  .delete = _global_delete,
  .join = _global_join,
  .leave = _global_leave,
  .free = libhpx_global_free,
  .malloc = libhpx_global_malloc,
  .calloc = libhpx_global_calloc,
  .memalign = libhpx_global_memalign
};

address_space_t *
address_space_new_jemalloc_global(const struct config *UNUSED) {
  return &_global_address_space_vtable;
}
