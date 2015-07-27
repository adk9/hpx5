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

#include <string.h>
#include <libhpx/debug.h>
#include <libhpx/memory.h>
#include <libhpx/system.h>
#include <malloc-2.8.6.h>
#include "registered.h"
#include "xport.h"

void
registered_allocator_init(pwc_xport_t *xport) {
  size_t bytes = 256 * 1024 * 1024; // arbitrary, for now.
  void *base = system_mmap_huge_pages(NULL, NULL, bytes, sizeof(bytes));
  if (!base) {
    dbg_error("failed to mmap %zu bytes anywhere in memory\n", bytes);
  }

  memset(base, 0, bytes);
  xport->pin(base, bytes, NULL);

  mspaces[AS_REGISTERED] = create_mspace_with_base(base, bytes, 0);
}
