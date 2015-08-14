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

#ifndef HPX_GAS_H
#define HPX_GAS_H

/// @defgroup agas Global Address Space
/// @brief Functions and definitions for using the global address space
/// @{

/// @file  include/hpx/gas.h
/// @brief Functions for allocating and using memory in the HPX global address
///        space.
#include <hpx/addr.h>

/// Global address-space layout and distribution.
typedef enum {
  HPX_DIST_TYPE_USER = 0,  //!< User-defined distribution.
  HPX_DIST_TYPE_LOCAL,     //!< Allocation local to calling locality.
  HPX_DIST_TYPE_CYCLIC,    //!< Cyclic distribution.
  HPX_DIST_TYPE_BLOCKED,   //!< Blocked sequential distribution.
} hpx_gas_dist_type_t;

/// User-defined GAS distribution function.
typedef hpx_addr_t (*hpx_gas_dist_t)(uint32_t i, size_t n, uint32_t bsize);

#define HPX_GAS_DIST_LOCAL   (hpx_gas_dist_t)HPX_DIST_TYPE_LOCAL
#define HPX_GAS_DIST_CYCLIC  (hpx_gas_dist_t)HPX_DIST_TYPE_CYCLIC
#define HPX_GAS_DIST_BLOCKED (hpx_gas_dist_t)HPX_DIST_TYPE_BLOCKED

/// Allocate distributed global memory given a distribution.
///
hpx_addr_t hpx_gas_alloc(size_t n, uint32_t bsize, uint32_t boundary,
                         hpx_gas_dist_t dist);

/// Allocate distributed zeroed global memory given a distribution.
///
hpx_addr_t hpx_gas_calloc(size_t n, uint32_t bsize, uint32_t boundary,
                          hpx_gas_dist_t dist);

/// Allocate cyclically distributed global memory.
///
/// This is not a collective operation; the returned address is returned only to
/// the calling thread, and must either be written into already-allocated global
/// memory, or sent via a parcel, for anyone else to address the allocation.
///
/// The total amount of usable memory allocated is @p n * @p bsize.
///
/// The alignment of each block (and thus the base alignment of the entire
/// array), will be 2^{align=ceil_log2_32(bsize)}, i.e., the minimum power of 2 to
/// bsize such that align >= bsize.
///
/// In UPC-land, the returned global address would have the following
/// distribution:
///
///    shared [bytes] char foo[n * bytes];
///
/// @param            n The number of blocks to allocate.
/// @param        bsize The number of bytes per block.
/// @param     boundary The alignment (2^k).
///
/// @returns            The global address of the allocated memory.
hpx_addr_t hpx_gas_alloc_cyclic(size_t n, uint32_t bsize, uint32_t boundary);

/// Allocate cyclically distributed global zeroed memory.
///
/// This call is similar to hpx_gas_alloc_cyclic except that the
/// global memory returned is initialized to 0.
///
/// @param            n The number of blocks to allocate.
/// @param        bsize The number of bytes per block.
/// @param     boundary The alignment (2^k).
///
/// @returns            The global address of the allocated memory.
hpx_addr_t hpx_gas_calloc_cyclic(size_t n, uint32_t bsize, uint32_t boundary);

/// Allocate distributed global memory laid out in a
/// super-block-cyclic manner where the size of each super-block is
/// equal to @p n/HPX_LOCALITIES.
///
/// This is not a collective operation; the returned address is returned only to
/// the calling thread, and must either be written into already-allocated global
/// memory, or sent via a parcel, for anyone else to address the allocation.
///
/// The total amount of usable memory allocated is @p n * @p bsize.
///
/// @param            n The number of blocks to allocate.
/// @param        bsize The number of bytes per block.
/// @param     boundary The alignment (2^k).
///
/// @returns            The global address of the allocated memory.
hpx_addr_t hpx_gas_alloc_blocked(size_t n, uint32_t bsize, uint32_t boundary);

/// Allocate partitioned, super-block-cyclically distributed global
/// zeroed memory.
///
/// This call is similar to hpx_gas_alloc_blocked except that the
/// global memory returned is initialized to 0.
///
/// @param            n The number of blocks to allocate.
/// @param        bsize The number of bytes per block.
/// @param     boundary The alignment (2^k).
///
/// @returns            The global address of the allocated memory.
hpx_addr_t hpx_gas_calloc_blocked(size_t n, uint32_t bsize, uint32_t boundary);

/// Allocate a block of global memory.
///
/// This is a non-collective call to allocate memory in the global
/// address space that can be moved. The allocated memory, by default,
/// has affinity to the allocating node, however in low memory conditions the
/// allocated memory may not be local to the caller. As it allocated in the GAS,
/// it is accessible from any locality, and may be relocated by the
/// runtime.
///
/// @param        bytes The number of bytes to allocate.
/// @param     boundary The alignment (2^k).
///
/// @returns            The global address of the allocated memory.
hpx_addr_t hpx_gas_alloc_local(uint32_t bytes, uint32_t boundary);
hpx_addr_t hpx_gas_alloc_local_at_sync(uint32_t bytes, uint32_t boundary, hpx_addr_t loc);
void hpx_gas_alloc_local_at_async(uint32_t bytes, uint32_t boundary, hpx_addr_t loc,
                                  hpx_addr_t lco);
extern HPX_ACTION_DECL(hpx_gas_alloc_local_at_action);

/// Allocate a 0-initialized block of global memory.
///
/// This is a non-collective call to allocate memory in the global address space
/// that can be moved. The allocated memory, by default, has affinity to the
/// allocating node, however in low memory conditions the allocated memory may
/// not be local to the caller. As it allocated in the GAS, it is accessible
/// from any locality, and may be relocated by the runtime.
///
/// *Note however that we do not track the alignment of allocations.* Users
/// should make sure to preserve alignment during move.
///
/// @param        nmemb The number of elements to allocate.
/// @param         size The number of bytes per element
/// @param     boundary The alignment (2^k).
///
/// @returns            The global address of the allocated memory.
hpx_addr_t hpx_gas_calloc_local(size_t nmemb, size_t size, uint32_t boundary);
hpx_addr_t hpx_gas_calloc_local_at_sync(size_t nmemb, size_t size, uint32_t boundary,
                                        hpx_addr_t loc);
void hpx_gas_calloc_local_at_async(size_t nmemb, size_t size, uint32_t boundary,
                                   hpx_addr_t loc, hpx_addr_t out);
extern HPX_ACTION_DECL(hpx_gas_calloc_local_at_action);

/// Free a global allocation.
///
/// This global free is asynchronous. The @p sync LCO address can be used to
/// test for completion of the free.
///
/// @param         addr The global address of the memory to free.
/// @param        rsync An LCO we can use to detect that the free has occurred.
void hpx_gas_free(hpx_addr_t addr, hpx_addr_t rsync);
void hpx_gas_free_sync(hpx_addr_t addr);

/// Change the locality-affinity of a global distributed memory address.
///
/// This operation is only valid in the AGAS GAS mode. For PGAS, it is effectively
/// a no-op.
///
/// @param          src The source address to move.
/// @param          dst The address pointing to the target locality to move the
///                       source address @p src to.
/// @param[out]     lco LCO object to check to wait for the completion of move.
void hpx_gas_move(hpx_addr_t src, hpx_addr_t dst, hpx_addr_t lco);

/// Performs address translation.
///
/// This will try to perform a global-to-local translation on the global @p
/// addr, and set @p local to the local address if it is successful. If @p
/// local is NULL, then this only performs address translation.
///
/// If the address is not local, will return false. Or, if @p local is not
/// NULL and the pin fails, this will return false, otherwise it will return
/// true.
///
/// Successful pinning operations must be matched with an unpin operation, if
/// the underlying data is ever to be moved.
///
/// @param         addr The global address.
/// @param[out]   local The pinned local address.
///
/// @returns       true If @p addr is local and @p local is NULL
///                true If @p addr is local and @p local is not NULL and pin is
///                       successful.
///               false If @p is not local.
///               false If @p is local and @local is not NULL and pin fails.
bool hpx_gas_try_pin(hpx_addr_t addr, void **local);

/// Unpin a previously pinned block.
///
/// @param         addr The address of global memory to unpin.
void hpx_gas_unpin(hpx_addr_t addr);

/// Allocate local memory for use in the memget/memput functions. Any memory can
/// be used in these functions, however only thread stacks and buffers allocated
/// with hpx_malloc_registered() are considered to be fast sources or targets
/// for these operations.
///
/// @param        bytes The number of bytes to allocate.
///
/// @returns            The buffer, or NULL if there was an error.
void *hpx_malloc_registered(size_t bytes);

/// Free local memory that was allocated with hpx_malloc_registered().
///
/// @param            p The buffer.
void hpx_free_registered(void *p);

/// This copies data from a global address to a local buffer, asynchronously.
///
/// The global address range [from, from + size) must be within a single block
/// allocated using one of the family of GAS allocation routines. This
/// requirement may not be checked. Copying data across a block boundary, or
/// from unallocated memory, will result in undefined behavior.
///
/// This operation is not atomic. memgets with concurrent memputs to overlapping
/// addresses ranges will result in a data race with undefined behavior. Users
/// should synchronize with some out-of-band mechanism.
///
/// @param           to The local address to copy to, must be a stack location
///                       or an address allocated with hpx_malloc_registered().
/// @param         from The global address to copy from.
/// @param         size The size, in bytes, of the buffer to copy
/// @param        lsync The address of a zero-sized future that can be used to
///                       wait for completion of the memget.
///
/// @returns HPX_SUCCESS
int hpx_gas_memget(void *to, hpx_addr_t from, size_t size, hpx_addr_t lsync);

/// Synchronous interface to memget.
int hpx_gas_memget_sync(void *to, hpx_addr_t from, size_t size);

/// This copies data from a local buffer to a global address, asynchronously.
///
/// The global address range [to, to + size) must be within a single block
/// allocated using one of the family of GAS allocation routines. This
/// requirement is not checked. Copying data across a block boundary, or to
/// unallocated memory, will result in undefined behavior.
///
/// This operation is not atomic. Concurrent memputs to overlapping addresses
/// ranges will result in a data race with undefined behavior. Users should
/// synchronize with some out-of-band mechanism.
///
/// @note A set to @p rsync implies @p lsync has also been set.
///
/// @param           to The global address to copy to.
/// @param         from The local address to copy from, must be a stack location
///                       or an address allocated with hpx_malloc_registered().
/// @param         size The size, in bytes, of the buffer to copy
/// @param        lsync The address of a zero-sized future that can be used to
///                       wait for local completion of the memput. Once this is
///                       signaled the @p from buffer may be reused or freed.
/// @param        rsync The address of a zero-sized future that can be used to
///                       wait for remote completion of the memput. Once this is
///                       signaled the put has become globally visible.
///
/// @returns  HPX_SUCCESS
int hpx_gas_memput(hpx_addr_t to, const void *from, size_t size,
                   hpx_addr_t lsync, hpx_addr_t rsync);

/// This copies data from a local buffer to a global address with locally
/// synchronous semantics.
///
/// This shares the same functionality as hpx_gas_memput(), but will not return
/// until the local @p from buffer can be reused. This exposes the potential for
/// a more efficient mechanism for synchronous operation, and should be
/// preferred where locally-synchronous semantics are desired.
///
/// @param           to The global address to copy to.
/// @param         from The local address to copy from, must be a stack location
///                       or an address allocated with hpx_malloc_registered().
/// @param         size The size, in bytes, of the buffer to copy
/// @param        rsync The address of a zero-sized future that can be used to
///                       wait for remote completion of the memput. Once this is
///                       signaled the put has become globally visible.
///
/// @returns  HPX_SUCCESS
int hpx_gas_memput_lsync(hpx_addr_t to, const void *from, size_t size,
                         hpx_addr_t rsync);

/// This copies data synchronously from a local buffer to a global address.
///
/// This shares the same functionality as hpx_gas_memput(), but will not return
/// until the write has completed remotely. This exposes the potential for
/// a more efficient mechanism for synchronous operation, and shoudl be
/// preferred where fully synchronous semantics are necessary.
///
/// @param           to The global address to copy to.
/// @param         from The local address to copy from, must be a stack location
///                       or an address allocated with hpx_malloc_registered().
/// @param         size The size, in bytes, of the buffer to copy
///
/// @returns  HPX_SUCCESS
int hpx_gas_memput_rsync(hpx_addr_t to, const void *from, size_t size);

/// This copies data from a global address to a global address, asynchronously.
///
/// The global address range [from, from + size) and [to, to + size) must be
/// within single blocks, respectively, allocated using one of the family of GAS
/// allocation routines. This requirement may not be checked. Copying data
/// across a block boundary, or from unallocated memory, will result in
/// undefined behavior.
///
/// This operation is not atomic. Concurrent memcpys to overlapping addresses
/// ranges will result in a data race with undefined behavior. Users should
/// synchronize with some out-of-band mechanism. Concurrent memcpys from
/// overlapping regions will be race free, as long as no concurrent memputs or
/// memcpys occur to that region.
///
/// @param           to The global address to copy to.
/// @param         from The global address to copy from.
/// @param         size The size, in bytes, of the buffer to copy
/// @param         sync The address of a zero-sized future that can be used to
///                       wait for completion of the memcpy.
///
/// @returns  HPX_SUCCESS
int hpx_gas_memcpy(hpx_addr_t to, hpx_addr_t from, size_t size, hpx_addr_t sync);

/// @}

#endif
