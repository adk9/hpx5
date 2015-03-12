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

void *_mmap_aligned(void *addr, size_t n, int prot, int flags, int fd, int off,
                    size_t align) {
  // overallocate space so that we have enough to guarantee that an aligned
  // address exists
  void *p = mmap(addr, n + align, prot, flags, fd, off);
  if (!p) {
    dbg_error("could not map %zu bytes with %zu alignment\n", n, align);
  }

  // find the range in the allocation that matches what we want
  const uintptr_t mask = (align - 1);
  uintptr_t suffix = (uintptr_t)p & mask;
  uintptr_t prefix = align - suffix;

  // the range is now partitioned into three bits,
  // [0, prefix) [prefix, prefix + n) [prefix + n, prefix + suffix + n)
  // we'll return [prefix, prefix + n)
  char *q = (char*)p + prefix;

  // return the overallocated pages back to the OS
  if (prefix) {
    system_munmap(p, prefix);
  }
  if (suffix) {
    system_munmap(q + n, suffix);
  }

  // and return the correctly aligned range
  return q;
}

void *system_mmap(void *addr, size_t n, size_t align) {
  static const int prot = PROT_READ | PROT_WRITE;
  const       int flags = MAP_ANONYMOUS | MAP_PRIVATE;
  const  uintptr_t mask = align - 1;

  void *p = mmap(addr, n, prot, flags, -1, 0);
  if ((uintptr_t)p & mask) {
    system_munmap(p, n);
    return _mmap_aligned(addr, n, prot, flags, -1, 0, align);
  }
  else {
    return p;
  }
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
