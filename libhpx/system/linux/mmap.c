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
#ifdef HAVE_HUGETLBFS
# include <hugetlbfs.h>
#endif
#include <sys/mman.h>
#include <libhpx/debug.h>
#include <libhpx/system.h>

/// A simple mmap wrapper that guarantees alignment.
///
/// This calls munmap at least once, and will attempt to allocate more data than
/// is always necessary, so it should only be used on a fallback path. This
/// implementation simply over allocates space so that we are guaranteed that
/// there is a properly aligned region inside of the mapping. It then munmaps
/// the parts of the allocation that aren't part of this region.
///
/// @param         addr A "suggested" address. This is most likely ignored.
/// @param            n The number of bytes to map, must be 2^n.
/// @param         prot The protection flags.
/// @param        flags Additional flags for the mapping.
/// @param           fd A file descriptor to map from.
/// @param          off The file offset to map at.
/// @param        align The alignment, must be 2^n.
///
/// @returns The properly-aligned mapped region, or NULL if there was an error.
static void *_mmap_aligned(void *addr, size_t n, int prot, int flags, int fd,
                           int off, size_t align) {
  char *buffer = mmap(addr, n + align, prot, flags, fd, off);
  if (buffer == MAP_FAILED) {
    dbg_error("could not map %zu bytes with %zu alignment\n", n, align);
  }

  // find the range in the allocation that matches what we want
  uintptr_t   bits = (uintptr_t)buffer;
  uintptr_t   mask = (align - 1);
  uintptr_t suffix = bits & mask;
  uintptr_t prefix = align - suffix;

  // return the overallocated pages back to the OS
  if (prefix) {
    system_munmap(NULL, buffer, prefix);
  }
  if (suffix) {
    system_munmap(NULL, buffer + prefix + n, suffix);
  }

  // and return the correctly aligned range
  return buffer + prefix;
}

/// This mmap wrapper tries once to mmap, and then forwards to _mmap_aligned().
///
/// @param         addr A "suggested" address. This is most likely ignored.
/// @param            n The number of bytes to map, must be 2^n.
/// @param         prot The protection flags.
/// @param        flags Additional flags for the mapping.
/// @param           fd A file descriptor to map from.
/// @param          off The file offset to map at.
/// @param        align The alignment, must be 2^n.
///
/// @returns The properly-aligned mapped region, or NULL if there was an error.
static void *_mmap_lucky(void *addr, size_t n, int prot, int flags, int fd,
                         int off, size_t align) {
  void *buffer = mmap(addr, n, prot, flags, fd, off);
  if (buffer == MAP_FAILED) {
    dbg_error("could not mmap %zu bytes from file %d\n", n, fd);
  }

  uintptr_t   bits = (uintptr_t)buffer;
  uintptr_t   mask = align - 1;
  uintptr_t modulo = bits & mask;
  if (!modulo) {
    return buffer;
  }

  system_munmap(NULL, buffer, n);
  return _mmap_aligned(addr, n, prot, flags, fd, off, align);
}

void *system_mmap(void *UNUSED, void *addr, size_t n, size_t align) {
  static const  int prot = PROT_READ | PROT_WRITE;
  static const int flags = MAP_ANONYMOUS | MAP_PRIVATE;
  return _mmap_lucky(addr, n, prot, flags, -1, 0, align);
}

void *system_mmap_huge_pages(void *UNUSED, void *addr, size_t n, size_t align) {
#ifndef HAVE_HUGETLBFS
  return system_mmap(UNUSED, addr, n, align);
#else
  static const int  prot = PROT_READ | PROT_WRITE;
  static const int flags = MAP_PRIVATE;
  long hugepagesize = gethugepagesize();
  long hugepagemask = hugepagesize - 1;
  if (align & hugepagemask) {
    log_mem("increasing alignment from %zu to %ld in huge page allocation\n",
            align, hugepagesize);
    align = hugepagesize;
  }
  if (n & hugepagemask) {
    long r = n & hugepagemask;
    long padding = hugepagesize - r;
    log_mem("adding %ld bytes to huge page allocation request\n", padding);
    n += padding;
  }
  int fd = hugetlbfs_unlinked_fd();
  dbg_assert_str(fd > 0, "could not get huge tlb file descriptor.");
  return _mmap_lucky(addr, n, prot, flags, fd, 0, align);
#endif
}

void system_munmap(void *UNUSED, void *addr, size_t size) {
  const long hugepagesize = gethugepagesize();
  int e = munmap(addr, size
#ifdef HAVE_HUGETLBFS
        + (size < hugepagesize ? (hugepagesize - (size % hugepagesize)) : 0)
#endif
  );
  if (e < 0) {
    dbg_error("munmap failed: %s.  addr is %"PRIuPTR", and size is %zu\n",
	      strerror(errno), (uintptr_t)addr, size);
  }
}
