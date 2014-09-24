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
#ifndef HPX_GAS_H
#define HPX_GAS_H

/// @file
/// @brief Functions for allocating and using memory in the HPX global address
///        space

#include "hpx/addr.h"

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
/// @param       addr the global address
/// @param[out] local the pinned local address
/// @returns
///                   - true; if @p addr is local and @p local is NULL
///                   - true; if @p addr is local and @p local is not NULL
///                           and pin is successful
///                   - false; if @p is not local
///                   - false; if @p is local and @local is not NULL and pin
///                            fails
bool hpx_gas_try_pin(const hpx_addr_t addr, void **local);


/// Allows an address to be remapped.
/// @param addr the address of global memory to unpin
void hpx_gas_unpin(const hpx_addr_t addr);


/// Allocate distributed global memory.
///
/// This is not a collective operation; the returned address is returned only to
/// the calling thread, and must either be written into already-allocated global
/// memory, or sent via a parcel, for anyone else to address the allocation.
///
/// In UPC-land, the returned global address would have the following
/// distribution:
///
///    shared [bytes] char foo[n * bytes];
///
/// @param         n the number of localities to allocated memory at
/// @param     bytes the number of bytes to allocate at each locality
/// @returns         the global address of the allocated memory
hpx_addr_t hpx_gas_global_alloc(size_t n, uint32_t bytes);


/// Allocate distributed global zeroed-memory.
///
/// This call is similar to hpx_gas_global_alloc except that the
/// global memory returned is set to 0.
///
/// @param         n the number of localities to allocated memory at
/// @param     bytes the number of bytes to allocate at each locality
/// @returns         the global address of the allocated memory
hpx_addr_t hpx_gas_global_calloc(size_t n, uint32_t bytes);


/// Allocate a block of global memory.
///
/// This is a non-collective call to allocate memory in the global
/// address space that can be moved. The allocated memory, by default,
/// has affinity to the allocating node. As it allocated in the GAS,
/// it is accessible from any locality, and may be relocated by the
/// runtime.
///
/// The total amount of memory allocated is bytes.
///
/// @param bytes the number of bytes per block to allocate
/// @returns     the global address of the allocated memory
hpx_addr_t hpx_gas_alloc(uint32_t bytes);


/// Free a global allocation.
///
/// This global free is asynchronous. The @p sync LCO address can be used to
/// test for completion of the free.
///
/// @param addr - the global address of the memory to free
/// @param sync - an LCO we can use to detect that the free has occurred
void hpx_gas_free(hpx_addr_t addr, hpx_addr_t sync);


/// Change the locality-affinity of a global distributed memory address.
///
/// This operation is only valid in the AGAS GAS mode. For PGAS, it is effectively
/// a no-op.
///
/// @param         src - the source address to move
/// @param         dst - the address pointing to the target locality to move the
///                      source address @p src to
/// @param[out]    lco - LCO object to check to wait for the completion of move
void hpx_gas_move(hpx_addr_t src, hpx_addr_t dst, hpx_addr_t lco);


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
/// @param             to The local address to copy to.
/// @param           from The global address to copy from.
/// @param           size The size, in bytes, of the buffer to copy
/// @param          lsync The address of a zero-sized future that can be used to
///                       wait for completion of the memget.
///
/// @returns HPX_SUCCESS
int hpx_gas_memget(void *to, hpx_addr_t from, size_t size, hpx_addr_t lsync);


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
/// @param             to The global address to copy to.
/// @param           from The local address to copy from.
/// @param           size The size, in bytes, of the buffer to copy
/// @param          lsync The address of a zero-sized future that can be used to
///                       wait for local completion of the memput. Once this is
///                       signaled the @p from buffer may be reused or freed.
/// @param          rsync The address of a zero-sized future that can be used to
///                       wait for remote completion of the memput. Once this is
///                       signaled the put has become globally visible.
///
/// @returns HPX_SUCCESS
int hpx_gas_memput(hpx_addr_t to, const void *from, size_t size,
                   hpx_addr_t lsync, hpx_addr_t rsync);


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
/// @param             to The global address to copy to.
/// @param           from The global address to copy from.
/// @param           size The size, in bytes, of the buffer to copy
/// @param           sync The address of a zero-sized future that can be used to
///                       wait for completion of the memcpy.
///
/// @returns HPX_OK
int hpx_gas_memcpy(hpx_addr_t to, hpx_addr_t from, size_t size, hpx_addr_t sync);

#endif
