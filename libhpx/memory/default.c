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

/// @file  libhpx/memory/default.c
///
/// @brief This file contains a default implementation of an address space that
///        just forwards to libc.
#include <stdlib.h>
#include <libhpx/memory.h>

address_space_t *local = NULL;
address_space_t *global = NULL;
address_space_t *registered = NULL;

static void *_default_memalign(size_t boundary, size_t size) {
  void *p;
  int e = posix_memalign(&p, boundary, size);
  dbg_assert(!e);
  return p;
  (void)e;
}

address_space_t *
address_space_new_default(const struct config *UNUSED) {
  address_space_t *space = malloc(sizeof(*space));
  dbg_assert(space);
  space->delete = free;
  space->free = free;
  space->malloc = malloc;
  space->calloc = calloc;
  space->memalign = _default_memalign;
  return space;
}
