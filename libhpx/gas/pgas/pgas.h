// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#ifndef LIBHPX_GAS_PGAS_H
#define LIBHPX_GAS_PGAS_H

#include <hpx/hpx.h>
#include <hpx/attributes.h>
#include <hpx/builtins.h>

extern struct heap *global_heap;

/// Called by each OS thread (pthread) to initialize and setup the thread local
/// structures required for it to use PGAS.
///
/// @note This currently only works with the global_heap object. A more flexible
///       solution might be appropriate some day.
///
/// @note Implemented in malloc.c.
///
/// @returns LIBHPX_OK, or LIBHPX_ERROR if there is an error.
///
int pgas_join(void)
  HPX_INTERNAL;

/// Called by each OS thread (pthread) to clean up any thread local structures
/// that were initialized during pgas_join().
///
/// @note Implemented in malloc.c.
///
void pgas_leave(void)
  HPX_INTERNAL;

/// Global versions of the standard malloc() functions.
///
/// These have exactly the same semantics as the traditional set of malloc
/// functions, except that they allocate their memory from the global heap. This
/// memory all has a GVA equivalent (@see heap.h pgas_lva_to_gva()) and can be
/// used in the network. It can also be shared directly between threads on an
/// SMP.
///
/// @note Implemented in malloc.c
/// @{
void *pgas_global_malloc(size_t bytes)
  HPX_INTERNAL;

void pgas_global_free(void *ptr)
  HPX_INTERNAL;

void *pgas_global_calloc(size_t nmemb, size_t size)
  HPX_INTERNAL;

void *pgas_global_realloc(void *ptr, size_t size)
  HPX_INTERNAL;

void *pgas_global_valloc(size_t size)
  HPX_INTERNAL;

void *pgas_global_memalign(size_t boundary, size_t size)
  HPX_INTERNAL;

int pgas_global_posix_memalign(void **memptr, size_t alignment, size_t size)
  HPX_INTERNAL;
/// @}

/// PGAS-specific local versions of the standard malloc() functions.
///
/// These have exactly the same semantics as the traditional set. The resulting
/// memory cannot be used by the network, but could be shared between threads
/// within an SMP. These are necessary because we override the standard
/// interface functions to call GAS versions since each GAS implementation does
/// things differently.
///
/// @note Implemented in malloc.c
/// @{
void *pgas_local_malloc(size_t bytes)
  HPX_INTERNAL;

void pgas_local_free(void *ptr)
  HPX_INTERNAL;

void *pgas_local_calloc(size_t nmemb, size_t size)
  HPX_INTERNAL;

void *pgas_local_realloc(void *ptr, size_t size)
  HPX_INTERNAL;

void *pgas_local_valloc(size_t size)
  HPX_INTERNAL;

void *pgas_local_memalign(size_t boundary, size_t size)
  HPX_INTERNAL;

int pgas_local_posix_memalign(void **memptr, size_t alignment, size_t size)
  HPX_INTERNAL;
/// @}

/// Implementation of the distributed functionality that supports cyclic malloc.
/// @{

typedef struct {
  size_t n;
  uint32_t bsize;
} pgas_alloc_args_t;

extern hpx_action_t pgas_cyclic_alloc;
extern hpx_action_t pgas_cyclic_calloc;
extern hpx_action_t pgas_free;
extern hpx_action_t pgas_set_csbrk;

hpx_addr_t pgas_cyclic_alloc_sync(size_t n, uint32_t bsize) HPX_INTERNAL;
hpx_addr_t pgas_cyclic_calloc_sync(size_t n, uint32_t bsize) HPX_INTERNAL;
/// @}

/// Register actions required by PGAS.
void pgas_register_actions(void)
  HPX_INTERNAL;

#endif // LIBHPX_GAS_PGAS_H
