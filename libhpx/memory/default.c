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
#include <libhpx/debug.h>
#include <libhpx/memory.h>

address_space_t *local = NULL;
address_space_t *global = NULL;
address_space_t *registered = NULL;

static address_space_t _default_address_space_vtable;

static void *_default_memalign(size_t boundary, size_t size) {
  void *p;
  int e = posix_memalign(&p, boundary, size);
  dbg_assert(!e);
  return p;
  (void)e;
}

static void _default_delete(void *space) {
  dbg_assert(space == &_default_address_space_vtable);
}

static void _default_join(void *space) {
  dbg_assert(space == &_default_address_space_vtable);
}

static void _default_leave(void *space) {
  dbg_assert(space == &_default_address_space_vtable);
}

static address_space_t _default_address_space_vtable = {
  .delete = _default_delete,
  .join = _default_join,
  .leave = _default_leave,
  .free = free,
  .malloc = malloc,
  .calloc = calloc,
  .memalign = _default_memalign
};

address_space_t *address_space_new_default(const struct config *UNUSED) {
  return &_default_address_space_vtable;
}
