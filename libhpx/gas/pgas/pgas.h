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

#ifndef LIBHPX_GAS_PGAS_H
#define LIBHPX_GAS_PGAS_H

#include <hpx/hpx.h>
#include <hpx/attributes.h>
#include <libhpx/network.h>

struct config;
struct gas;

struct gas *gas_pgas_new(const struct config *cfg, struct boot *boot);

/// Called by each OS thread (pthread) to initialize and setup the thread local
/// structures required for it to use PGAS.
///
/// @note This currently only works with the global_heap object. A more flexible
///       solution might be appropriate some day.
///
/// @note Implemented in malloc.c.
///
/// @returns LIBHPX_OK, or LIBHPX_ERROR if there is an error.
int pgas_join(void);

/// Called by each OS thread (pthread) to clean up any thread local structures
/// that were initialized during pgas_join().
///
/// @note Implemented in malloc.c.
void pgas_leave(void);

/// Implementation of the distributed functionality that supports cyclic malloc
/// and calloc.
/// @{

/// Asynchronous entry point for alloc.
/// type hpx_addr_t (size_t bytes, size_t align)
extern HPX_ACTION_DECL(pgas_alloc_cyclic);

/// Asynchronous entry point for calloc.
/// type hpx_addr_t (size_t bytes, size_t align)
extern HPX_ACTION_DECL(pgas_calloc_cyclic);

/// Asynchronous entry point for free.
extern HPX_ACTION_DECL(pgas_free);

/// Asynchronous entry point for the rsync handler for memput
/// void (int src, uint64_t command)
// extern COMMAND_DECL(memput_rsync);

/// Synchronous entry point for alloc.
///
/// @param            n The total number of bytes to allocate.
/// @param        bsize The size of each block, in bytes.
///
/// @returns            A global address representing the base of the
///                     allocation, or HPX_NULL if there is an error.
hpx_addr_t pgas_alloc_cyclic_sync(size_t n, uint32_t bsize);

/// Synchronous entry point for calloc.
///
/// @param            n The total number of bytes to allocate.
/// @param        bsize The size of each block, in bytes.
///
/// @returns            A global address representing the base of the
///                     allocation, or HPX_NULL if there is an error.
hpx_addr_t pgas_calloc_cyclic_sync(size_t n, uint32_t bsize);

/// @}

/// Convert a global address into a local virtual address.
///
/// @param          gpa The global physical address.
///
/// @returns            The corresponding local virtual address.
void *pgas_gpa_to_lva(hpx_addr_t gpa);

/// Convert a heap offset into a local virtual address.
///
/// @param       offset The heap offset.
///
/// @returns            The corresponding local virtual address, or NULL if the
///                     offset is outside the bounds of the heap.
void *pgas_offset_to_lva(uint64_t offset);

/// Convert a local address into a global physical address.
///
/// @param          lva The local virtual address.
///
/// @returns            The corresponding local virtual address.
hpx_addr_t pgas_lva_to_gpa(const void *lva);

/// Get the current maximum heap offset.
uint64_t pgas_max_offset(void);

#endif // LIBHPX_GAS_PGAS_H
