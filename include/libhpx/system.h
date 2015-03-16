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
#ifndef LIBHPX_SYSTEM_H
#define LIBHPX_SYSTEM_H

#include <pthread.h>
#include <hpx/attributes.h>

int system_get_cores(void)
  HPX_INTERNAL;

int system_set_affinity(pthread_t thread, int core_id)
  HPX_INTERNAL;

int system_set_affinity_group(pthread_t thread, int ncores)
  HPX_INTERNAL;

/// Get the pthread's stack extent.
///
/// @param       thread The thread id to query.
/// @param[out]    base The bottom (lowest address) of the stack.
/// @param[out]    size The size of the stack.
void system_get_stack(pthread_t thread, void **base, size_t *size)
  HPX_INTERNAL;

/// An abstract interface to mmap-like operations.
///
/// As opposed to mmap, this guarantees alignment. It will try and place the
/// corresponding allocation at @p addr, but it won't try too hard.
///
/// @param          obj User data to match the object oriented mmap interface.
/// @param         addr A hint about where to try and place the allocation.
/// @param         size The size in bytes of the allocation (must be 2^n).
/// @param        align The alignment in bytes of the allocation (must be 2^n).
///
/// @returns The allocated region.
void *system_mmap(void *obj, void *addr, size_t bytes, size_t align)
  HPX_INTERNAL;

typedef void *(*system_mmap_t)(void *, void *, size_t, size_t);

/// An abstract interface to mmap-like operations for huge pages.
///
/// As opposed to mmap, this guarantees alignment. It will try and place the
/// corresponding allocation at @p addr, but it won't try too hard.
///
/// @param          obj User data to match the object oriented mmap interface.
/// @param         addr A hint about where to try and place the allocation.
/// @param         size The size in bytes of the allocation (must be 2^n).
/// @param        align The alignment in bytes of the allocation (must be 2^n).
///
/// @returns The allocated region.
void *system_mmap_huge_pages(void *obj, void *addr, size_t bytes, size_t align)
  HPX_INTERNAL;

/// Unmap memory.
void system_munmap(void *obj, void *addr, size_t size)
  HPX_INTERNAL;

typedef void (*system_munmap_t)(void *, void *, size_t);

#endif // LIBHPX_SYSTEM_H
