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
#ifndef HPX_LCO_H
#define HPX_LCO_H

/// \file

#include "hpx/addr.h"
#include "hpx/types.h"

// ----------------------------------------------------------------------------
/// LCOs are local control objects. All LCOs support the "set" and "delete"
/// action interface, while subclasses, might provide more specific interfaces.
/// @param  lco the lco to delete
/// @param sync the address of an LCO to set when deletion is complete;
///             may be HPX_NULL
// ----------------------------------------------------------------------------
void hpx_lco_delete(hpx_addr_t lco, hpx_addr_t sync);


// ----------------------------------------------------------------------------
/// Set an LCO with a status.
/// @param    lco the lco to set
/// @param  value the value to set the LCO with
/// @param   size the size of the @p value
/// @param status the status to set the LCO with
/// @param   sync address of an LCO to set when @p lco has been set;
///               may be HPX_NULL
// ----------------------------------------------------------------------------
void hpx_lco_set_status(hpx_addr_t lco, const void *value, int size,
                        hpx_status_t status, hpx_addr_t sync);


// ----------------------------------------------------------------------------
/// Set an LCO, optionally with data.
/// @param    lco the lco to set
/// @param  value the value to set the LCO to
/// @param   size the size of the @p value
/// @param   sync address of an LCO to set when @p lco has been set;
///               may be HPX_NULL
// ----------------------------------------------------------------------------
void hpx_lco_set(hpx_addr_t lco, const void *value, int size, hpx_addr_t sync);


// ----------------------------------------------------------------------------
/// Perform a wait operation.
///
/// The LCO blocks the caller until an LCO set operation signals the LCO. Each
/// LCO type has its own semantics for the state under which this occurs.
///
/// @param lco the LCO we're processing
/// @returns   the LCO's status
// ----------------------------------------------------------------------------
hpx_status_t hpx_lco_wait(hpx_addr_t lco);


// ----------------------------------------------------------------------------
/// Perform a get operation.
///
/// An LCO blocks the caller until the future is set, and then copies its value
/// data into the provided buffer.
///
/// @param      lco the LCO we're processing
/// @param[out] out the output location (may be null)
/// @param     size the size of the data
/// @returns        the LCO's status
// ----------------------------------------------------------------------------
hpx_status_t hpx_lco_get(hpx_addr_t lco, void *value, int size);


// ----------------------------------------------------------------------------
/// Blocks the thread until all of the LCOs have been set.
///
/// This admits some parallelism in the implementation, and is preferable to
/// using hpx_lco_wait() in a loop.
///
/// @param    n the number of LCOs in @p lcos
/// @param lcos an array of LCO addresses (must be uniformly non-HPX_NULL, and
///             correspond to global addresses associated with real LCOs)
// ----------------------------------------------------------------------------
void hpx_lco_wait_all(int n, hpx_addr_t lcos[]);


// ----------------------------------------------------------------------------
/// Blocks the thread until all of the LCOs have been set, returning their
/// values.
///
/// This admits some parallelism in the implementation, and is preferable to
/// using hpx_lco_get() in a loop.
///
/// @param           n the number of LCOs
/// @param        lcos an array of @p n global LCO addresses
/// @param[out] values an array of @p n local buffers with sizes corresponding
///                    to @p sizes
/// @param       sizes an @p n element array of sizes that must corrsepond to
///                    @p lcos and @p values
// ----------------------------------------------------------------------------
void hpx_lco_get_all(int n, hpx_addr_t lcos[], void *values[], int sizes[]);


// ----------------------------------------------------------------------------
/// Semaphores are builtin LCOs that represent resource usage.
///
/// @param init initial value semaphore will be created with
/// @returns    global address of semaphore
// ----------------------------------------------------------------------------
hpx_addr_t hpx_lco_sema_new(unsigned init);


// ----------------------------------------------------------------------------
/// Standard semaphore V (signal) operation.
///
/// Increments the count in the semaphore, signaling the LCO if the increment
/// transitions from 0 to 1. This is asynchronous, if the caller needs
/// synchronous operation then they should pass an LCO address as @p sync, and
/// wait on it.
///
/// @param sema the global address of a semaphore
/// @param sync the global address of an LCO to set after the operation is 
///             complete; may be HPX_NULL
// ----------------------------------------------------------------------------
void hpx_lco_sema_v(hpx_addr_t sema, hpx_addr_t sync);


// ----------------------------------------------------------------------------
/// Standard semaphore P (wait) operation.
///
/// Attempts to decrement the count in the semaphore; blocks if the count is 0.
///
/// @param sema the global address of a semaphore
// ----------------------------------------------------------------------------
void hpx_lco_sema_p(hpx_addr_t sema);


// ----------------------------------------------------------------------------
/// Create an "and" LCO.
///
/// An "and" LCO represents an AND gate.
/// 
/// @param inputs the number of LCOs to gate
/// @returns      the global address of the new "and" LCO
// ----------------------------------------------------------------------------
hpx_addr_t hpx_lco_and_new(uint64_t inputs);

// ----------------------------------------------------------------------------
/// Set an "and" LCO, triggering it (i.e. setting it) if appropriate.
///
/// If this set is the last one the "and" LCO is waiting on, the "and" LCO 
/// will be set.
/// 
/// @param  and the global address of the "and" LCO to set.
/// @param sync the address of an LCO to set when the "and" LCO is set;
///             may be HPX_NULL
// ----------------------------------------------------------------------------
void hpx_lco_and_set(hpx_addr_t and, hpx_addr_t sync); // async

// ----------------------------------------------------------------------------
/// Create a future.
///
/// Futures are builtin LCOs that represent values returned from asynchronous
/// computation.
///
/// @param size the size of the future's value (may be 0)
/// @returns    the glboal address of the newly allocated future
// ----------------------------------------------------------------------------
hpx_addr_t hpx_lco_future_new(int size);

// ----------------------------------------------------------------------------
/// Allocate a global array of futures.
///
/// Each of the futures needs to be initialized correctly, and if they need to
/// be out of place, then each locality needs to allocate the out-of-place size
/// required.
///
/// @param          n - the (total) number of futures to allocate
/// @param       size - the size of each futures' value
/// @param block_size - the number of futures per block
// ----------------------------------------------------------------------------
hpx_addr_t hpx_lco_future_array_new(int n, int size, int block_size);

// ----------------------------------------------------------------------------
/// Get an address of a future in a future array
/// 
/// @param base the base address of the array of futures
/// @param    i the index of the future to return
/// @returns    the address of the ith future in the array
// ----------------------------------------------------------------------------
hpx_addr_t hpx_lco_future_array_at(hpx_addr_t base, int i);

// ----------------------------------------------------------------------------
/// Delete an array of futures.
///
/// @param base the base address of the array of futures
/// @param sync optional LCO to set when deletion is complete; may be 
///             HPX_NULL
// ----------------------------------------------------------------------------
void hpx_lco_future_array_delete(hpx_addr_t base, hpx_addr_t sync);

// ----------------------------------------------------------------------------
// Channels.
// ----------------------------------------------------------------------------
/// @todo Channels documentation
hpx_addr_t hpx_lco_chan_new(void);
void hpx_lco_chan_send(hpx_addr_t chan, const void *value, int size, hpx_addr_t sync);
void *hpx_lco_chan_recv(hpx_addr_t chan, int size);

hpx_addr_t hpx_lco_chan_array_new(int n, int block_size);
hpx_addr_t hpx_lco_chan_array_at(hpx_addr_t base, int i);
void hpx_lco_chan_array_delete(hpx_addr_t array, hpx_addr_t sync);
hpx_status_t hpx_lco_chan_array_select(hpx_addr_t chans[], int n, int size, int *index,
                                       void **out);

#endif
