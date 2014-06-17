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

#include "hpx/addr.h"
#include "hpx/types.h"


/// ----------------------------------------------------------------------------
/// An action-based interface to the LCO set operation.
/// ----------------------------------------------------------------------------
extern hpx_action_t hpx_lco_set_action;

/// ----------------------------------------------------------------------------
/// LCO's are local control objects.
/// ----------------------------------------------------------------------------
void hpx_lco_delete(hpx_addr_t lco, hpx_addr_t sync);


/// ----------------------------------------------------------------------------
/// Propagate an error to an LCO.
///
/// @param  lco - the LCO's global address
/// @param code - a user-defined error code
/// @param sync - a future for local completion
/// ----------------------------------------------------------------------------
void hpx_lco_error(hpx_addr_t lco, hpx_status_t code, hpx_addr_t sync);


/// ----------------------------------------------------------------------------
/// Set an LCO, optionally with data.
///
/// @param   lco - the LCO to set
/// @param value - the address of the value to set
/// @param  size - the size of the data
/// @param  sync - a future for local completion
/// ----------------------------------------------------------------------------
void hpx_lco_set(hpx_addr_t lco, const void *value, int size, hpx_addr_t sync);


/// ----------------------------------------------------------------------------
/// Perform a wait operation.
///
/// The LCO blocks the caller until an LCO set operation triggers the LCO. Each
/// LCO type has its own semantics for the state under which this occurs.
///
/// If the return status is HPX_LCO_ERROR then the LCO was triggered by
/// hpx_lco_error() rather than hpx_lco_set().
///
/// @param lco - the LCO we're processing
/// @returns   - the LCO's status
/// ----------------------------------------------------------------------------
hpx_status_t hpx_lco_wait(hpx_addr_t lco);


/// ----------------------------------------------------------------------------
/// Perform a get operation.
///
/// An LCO blocks the caller until the future is set, and then copies its value
/// data into the provided buffer.
///
/// If the return status is HPX_LCO_ERROR then the LCO was triggered by
/// hpx_lco_error() rather than hpx_lco_set().
///
/// @param      lco - the LCO we're processing
/// @param[out] out - the output location (may be null)
/// @param     size - the size of the data
/// @returns        - the LCO's status
/// ----------------------------------------------------------------------------
hpx_status_t hpx_lco_get(hpx_addr_t lco, void *value, int size);


/// ----------------------------------------------------------------------------
/// Blocks the thread until all of the LCOs have been set.
///
/// This admits some parallelism in the implementation, and is preferable to
/// using hpx_lco_wait() in a loop.
///
/// @param    n - the number of LCOs in @p lcos
/// @param lcos - an array of LCO addresses (must be uniformly non-HPX_NULL, and
///               correspond to global addresses associated with real LCOs)
/// ----------------------------------------------------------------------------
void hpx_lco_wait_all(int n, hpx_addr_t lcos[]);


/// ----------------------------------------------------------------------------
/// Blocks the thread until all of the LCOs have been set, returning their
/// values.
///
/// This admits some parallelism in the implementation, and is preferable to
/// using hpx_lco_get() in a loop.
///
/// @param           n - the number of LCOs
/// @param        lcos - an array of @p n global LCO addresses
/// @param[out] values - an array of @p n local buffers with sizes corresponding
///                      to @p sizes
/// @param       sizes - an @p n element array of sizes that must corrsepond to
///                      @p lcos and @p values
/// ----------------------------------------------------------------------------
void hpx_lco_get_all(int n, hpx_addr_t lcos[], void *values[], int sizes[]);


/// ----------------------------------------------------------------------------
/// Semaphores are builtin LCOs that represent resource usage.
/// ----------------------------------------------------------------------------
hpx_addr_t hpx_lco_sema_new(unsigned init);


/// ----------------------------------------------------------------------------
/// Standard semaphore V operation.
///
/// Increments the count in the semaphore, signaling the LCO if the increment
/// transitions from 0 to 1. This is always asynchronous, i.e., returning from
/// this routine simply means that the increment has been scheduled.
///
/// @param sema - the global address of a semaphore
/// ----------------------------------------------------------------------------
void hpx_lco_sema_v(hpx_addr_t sema);


/// ----------------------------------------------------------------------------
/// Standard semaphore P operation.
///
/// Attempts to decrement the count in the semaphore, blocks if the count is 0.
///
/// @param sema - the global address of a semaphore
/// @returns    - HPX_SUCCESS, or an error code if the semaphore is in an error
///               condition
/// ----------------------------------------------------------------------------
hpx_status_t hpx_lco_sema_p(hpx_addr_t sema);


/// ----------------------------------------------------------------------------
/// An and LCO represents an AND gate.
/// ----------------------------------------------------------------------------
hpx_addr_t hpx_lco_and_new(uint64_t inputs);
void hpx_lco_and_set(hpx_addr_t and, hpx_addr_t sync); // async


/// ----------------------------------------------------------------------------
/// Futures are builtin LCOs that represent values returned from asynchronous
/// computation.
/// ----------------------------------------------------------------------------
hpx_addr_t hpx_lco_future_new(int size);

hpx_addr_t hpx_lco_future_array_new(int n, int size, int block_size);
hpx_addr_t hpx_lco_future_array_at(hpx_addr_t base, int i);
void hpx_lco_future_array_delete(hpx_addr_t array, hpx_addr_t sync);


/// ----------------------------------------------------------------------------
/// Channels.
/// ----------------------------------------------------------------------------
hpx_addr_t hpx_lco_chan_new(void);
void hpx_lco_chan_send(hpx_addr_t chan, const void *value, int size, hpx_addr_t sync);
void *hpx_lco_chan_recv(hpx_addr_t chan, int size);

hpx_addr_t hpx_lco_chan_array_new(int n, int block_size);
hpx_addr_t hpx_lco_chan_array_at(hpx_addr_t base, int i);
void hpx_lco_chan_array_delete(hpx_addr_t array, hpx_addr_t sync);
hpx_status_t hpx_lco_chan_array_select(hpx_addr_t chans[], int n, int size, int *index,
                                       void **out);

/// ----------------------------------------------------------------------------
/// Allocate a new generation counter.
///
/// A generation counter allows an application programmer to efficiently wait
/// for a counter. The @p depth indicates a bound on the typical number of
/// generations that are explicitly active---it does not impact correctness,
/// merely performance.
///
/// As an example, if there are typically three generations active (i.e.,
/// threads may exist for up to three generations ahead of the current
/// generation), then depth should be set to three. If it is set to two, then
/// the runtime will perform some extra work testing threads that should not be
/// tested.
///
/// @param depth - the typical number of active generations
/// ----------------------------------------------------------------------------
hpx_addr_t hpx_lco_gencount_new(unsigned int depth);

/// ----------------------------------------------------------------------------
/// Increment the generation counter.
///
/// @param gencnt - the counter to increment
/// ----------------------------------------------------------------------------
void hpx_lco_gencount_inc(hpx_addr_t gencnt);

/// ----------------------------------------------------------------------------
/// Wait for the generation counter to reach a certain value.
///
/// It is OK to wait for any generation. If the generation has already passed,
/// this will probably not block. If the generation is far in the future (far in
/// this case means more than the depth value provided in the counters
/// allocator) then the thread may (transparently) wake up more often than it
/// needs to.
///
/// When this returns, it is guaranteed that the current count is <= @p gen, and
/// progress is guaranteed (that is, all threads waiting for @p gen will run in
/// some bounded amount of time when the counter reaches @p gen).
///
/// @param gencnt - the counter to wait for
/// @param    gen - the generation to wait for
/// ----------------------------------------------------------------------------
void hpx_lco_gencount_wait(hpx_addr_t gencnt, unsigned long gen);

#endif
