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
#include <sys/mman.h>
#include <libhpx/debug.h>
#include <libhpx/system.h>

void *system_mmap(void *addr, size_t size, size_t align) {
  static const int prot  = PROT_READ | PROT_WRITE;
  static const int flags = MAP_ANONYMOUS | MAP_PRIVATE;

  void *p = mmap(addr, size, prot, flags, -1, 0);
  dbg_assert(p);
  return p;
}

void *system_mmap_huge_pages(void *addr, size_t size, size_t align) {
  return NULL;
}

void system_munmap(void *addr, size_t size) {
  int e = munmap(addr, size);
  if (e < 0) {
    dbg_error("munmap failed: %s.\n", strerror(e));
  }
}
