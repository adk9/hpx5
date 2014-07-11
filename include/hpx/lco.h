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

/// @file include/hpx/lco.h
/// @brief The HPX LCO interface.

#include "hpx/addr.h"
#include "hpx/types.h"


/// An action-based interface to the LCO set operation.
extern hpx_action_t hpx_lco_set_action;


/// Delete an LCO.
///
/// @param   lco the address of the LCO to delete
/// @param rsync an LCO to signal remote completion
void hpx_lco_delete(hpx_addr_t lco, hpx_addr_t rsync);


/// Propagate an error to an LCO.
///
/// @param   lco the LCO's global address
/// @param  code a user-defined error code
/// @param rsync an LCO to signal remote completion
void hpx_lco_error(hpx_addr_t lco, hpx_status_t code, hpx_addr_t rsync);


/// Set an LCO, optionally with data.
///
/// @param   lco the LCO to set
/// @param  size the size of the data
/// @param value the address of the value to set
/// @param lsync an LCO to signal local completion (HPX_NULL == don't wait)
///              (Local completion indicates that the @p value may be freed 
///              or reused.)
/// @param rsync an LCO to signal remote completion (HPX_NULL == don't wait)
void hpx_lco_set(hpx_addr_t lco, int size, const void *value, hpx_addr_t lsync,
                 hpx_addr_t rsync);

/// An action to set an LCO with arguments of type hpx_lco_set_args_t
extern hpx_action_t hpx_lco_set_action;

/// Perform a wait operation.
///
/// The LCO blocks the caller until an LCO set operation triggers the LCO. Each
/// LCO type has its own semantics for the state under which this occurs.
///
/// @param lco the LCO we're processing
/// @returns   HPX_SUCCESS or the code passed to hpx_lco_error()
hpx_status_t hpx_lco_wait(hpx_addr_t lco);


/// Perform a get operation.
///
/// An LCO blocks the caller until the future is set, and then copies its value
/// data into the provided buffer.
///
/// If the return status is not HPX_SUCCESS then the LCO was triggered by
/// hpx_lco_error() rather than hpx_lco_set(), in such a case the memory pointed
/// to by @p out will not be inspected.
///
/// @param      lco the LCO we're processing
/// @param     size the size of the data
/// @param[out] out the output location (may be null)
/// @returns        HPX_SUCCESS or the code passed to hpx_lco_error()
hpx_status_t hpx_lco_get(hpx_addr_t lco, int size, void *value);


/// Blocks the thread until all of the LCOs have been set.
///
/// This admits some parallelism in the implementation, and is preferable to
/// using hpx_lco_wait() in a loop.
///
/// @param             n the number of LCOs in @p lcos
/// @param          lcos an array of LCO addresses (must be uniformly
///                      non-HPX_NULL, and correspond to global addresses
///                      associated with real LCOs)
/// @param[out] statuses an array of statuses, pass NULL if statuses are not
///                      required
/// @returns             the number of entries in @p statuses that have
///                      non-HPX_SUCCESS values, will be set irrespective of if
///                      @p statuses is NULL
int hpx_lco_wait_all(int n, hpx_addr_t lcos[], hpx_status_t statuses[]);


/// Blocks the thread until all of the LCOs have been set, returning their
/// values.
///
/// This admits some parallelism in the implementation, and is preferable to
/// using hpx_lco_get() in a loop.
///
/// @param             n the number of LCOs
/// @param          lcos an array of @p n global LCO addresses
/// @param         sizes an @p n element array of sizes that must correspond to
///                      @p lcos and @p values
/// @param[out]   values an array of @p n local buffers with sizes corresponding
///                      to @p sizes
/// @param[out] statuses an array of statuses, pass NULL if statuses are not
///                      required
///
/// @returns The number of entries in @p statuses that have non-HPX_SUCCESS
///                        values, will be set irrespective of if @p statuses is
///                        NULL.
int hpx_lco_get_all(int n, hpx_addr_t lcos[], int sizes[], void *values[],
                     hpx_status_t statuses[]);


/// Semaphores are builtin LCOs that represent resource usage.
///
/// @param init initial value semaphore will be created with
///
/// @returns The global address of the new semaphore.
hpx_addr_t hpx_lco_sema_new(unsigned init);


/// Standard semaphore V (signal) operation.
///
/// Increments the count in the semaphore, signaling the LCO if the increment
/// transitions from 0 to 1. This is always asynchronous, i.e., returning from
/// this routine simply means that the increment has been scheduled.
///
/// @param sema the global address of a semaphore
void hpx_lco_sema_v(hpx_addr_t sema);


/// Standard semaphore P (wait) operation.
///
/// Attempts to decrement the count in the semaphore; blocks if the count is 0.
///
/// @param sema the global address of a semaphore
///
/// @returns HPX_SUCCESS, or an error code if the semaphore is in an error
///          condition
hpx_status_t hpx_lco_sema_p(hpx_addr_t sema);


/// An and LCO represents an AND gate.
/// @{

/// Create an AND gate.
///
/// @param inputs the number of inputs to the and (must be >= 0)
///
/// @returns The global address of the new and gate.
hpx_addr_t hpx_lco_and_new(intptr_t inputs);

/// Join an "and" LCO, triggering it (i.e. setting it) if appropriate.
///
/// If this set is the last one the "and" LCO is waiting on, the "and" LCO
/// will be set.
///
/// @param  and the global address of the "and" LCO to set.
/// @param sync the address of an LCO to set when the "and" LCO is set;
///             may be HPX_NULL
void hpx_lco_and_set(hpx_addr_t and, hpx_addr_t sync); // async
/// @}

/// Create a future.
///
/// Futures are builtin LCOs that represent values returned from asynchronous
/// computation.
/// Futures are always allocated in the global address space, because their
/// addresses are used as the targets of parcels.
///
/// @param size the size in bytes of the future's value (may be 0)
/// @returns    the glboal address of the newly allocated future
hpx_addr_t hpx_lco_future_new(int size);


/// Allocate a global array of futures.
///
/// @param          n The (total) number of futures to allocate
/// @param       size The size of each futures' value
/// @param block_size The number of futures per block
hpx_addr_t hpx_lco_future_array_new(int n, int size, int block_size);


/// Get an address of a future in a future array
///
/// @param base the base address of the array of futures
/// @param    i the index of the future to return
/// @returns    the address of the ith future in the array
hpx_addr_t hpx_lco_future_array_at(hpx_addr_t base, int i);


/// Channels.
///
/// The channel LCO approximates an MPI channel.
///
/// The channel send operation creates at least one copy of the sent buffer,
/// possibly asynchronously, and the channel recv operation returns a pointer to
/// this buffer (any intermediate copies are managed by the runtime).


/// Allocate a new channel.
///
/// @returns The global address of the newly allocated channel.
hpx_addr_t hpx_lco_chan_new(void);


/// Send a buffer through a channel.
///
/// Sending the @p buffer entails creating at least one copy of the buffer. The
/// @p lsync LCO will be set when the @p buffer can be safely written to. The @p
/// rsync LCO can be used to provide an ordered channel---if every send operation
/// waits on remote completion before sending on the same channel again then the
/// receives from the channel will occur in that same order. No ordering
/// guarantees are supplied between channels, or given an out-of-band
/// communication mechanism, other than those that can be deduced through
/// waiting on @p rsync.
///
/// The send operation is equivalent to hpx_lco_set() for the channel.
///
/// @param   chan The channel to use.
/// @param   size The size of the @p buffer.
/// @param buffer The buffer to send.
/// @param  lsync An LCO to signal on local completion (i.e., R/W access or free
///               to @p buffer is safe), HPX_NULL if we don't care.
/// @param  rsync An LCO to signal remote completion (i.e., the buffer has been
///               enqueued remotely), HPX_NULL if we don't care.
void hpx_lco_chan_send(hpx_addr_t chan, int size, const void *buffer,
                       hpx_addr_t lsync, hpx_addr_t rsync);



/// Send a buffer through an ordered channel.
///
/// All in order sends from a thread are guaranteed to be received in the order
/// that they were sent. No guarantee is made for inorder sends on the same
/// channel from different threads. More complicated ordering can be provided
/// using the @p rsync LCO and the hpx_lco_chan_send() interface.
///
/// @param   chan The channel to use.
/// @param   size The size of the @p buffer.
/// @param buffer The buffer to send.
/// @param  lsync An LCO to signal on local completion (i.e., R/W access or free
///               to @p buffer is safe), HPX_NULL if we don't care.
void hpx_lco_chan_send_inorder(hpx_addr_t chan, int size, const void *buffer,
                               hpx_addr_t lsync);


/// Receive a buffer from a channel.
///
/// This is a blocking call. The user is responsible for freeing the returned
/// buffer.
///
/// @param[in]    chan the global address of the channel to receive from
/// @param[out]   size the size of the received buffer
/// @param[out] buffer the address of the received buffer
///
/// @returns HPX_SUCCESS or an error code
hpx_status_t hpx_lco_chan_recv(hpx_addr_t chan, int *size, void **buffer);


/// Probe a single channel to attempt to read.
///
/// The hpx_lco_chan_recv() interface blocks the caller until a buffer is
/// available. This hpx_lco_chan_try_recv() operation will instead return
/// HPX_LCO_CHAN_EMPTY to indicate that no buffer was available.
///
/// @param[in]    chan the global address of the channel
/// @param[out]   size the size of the received buffer, if there was one
/// @param[out] buffer the received buffer, if there was one
///
/// @returns HPX_SUCCESS if a buffer was received,
///          HPX_LCO_CHAN_EMPY if there was no buffer available
///          HPX_LCO_ERROR if the channel has an error
hpx_status_t hpx_lco_chan_try_recv(hpx_addr_t chan, int *size, void **buffer);


/// Receive from one of a set of channels.
///
/// This is a blocking call, and the user is responsible for freeing the
/// returned buffer. The return value corresponds to the error condition for the
/// channel that we matched on, if it is not HPX_SUCCESS, then @p index
/// indicates the channel, but the @p size and @p buffer should not be
/// inspected.
///
/// @param[in]        n the number of channels we're probing
/// @param[in] channels an array of channels to probe
/// @param[out]   index the index of the channel that we matched
/// @param[out]    size the size of the buffer that we matched
/// @param[out]     out the buffer that we matched
///
/// @returns HPX_SUCCESS or an error code if the channel associated with @p
///          index has an error set.
hpx_status_t hpx_lco_chan_array_select(int n, hpx_addr_t channels[],
                                       int *index, int *size, void **out);

hpx_addr_t hpx_lco_chan_array_new(int n, int block_size);
hpx_addr_t hpx_lco_chan_array_at(hpx_addr_t base, int i);
void hpx_lco_chan_array_delete(hpx_addr_t array, hpx_addr_t sync);


/// Allocate a new generation counter.
///
/// A generation counter allows an application programmer to efficiently wait
/// for a counter. The @p ninplace indicates a bound on the typical number of
/// generations that are explicitly active---it does not impact correctness,
/// merely performance.
///
/// As an example, if there are typically three generations active (i.e.,
/// threads may exist for up to three generations ahead of the current
/// generation), then @p ninplace should be set to three. If it is set to two,
/// then the runtime will perform some extra work testing threads that should
/// not be tested.
///
/// @param ninplace the typical number of active generations
///
/// @returns The global address of the new generation count.
hpx_addr_t hpx_lco_gencount_new(unsigned long ninplace);


/// Increment the generation counter.
///
/// @param gencnt the counter to increment
/// @param  rsync The global address of an LCO signal remote completion.
void hpx_lco_gencount_inc(hpx_addr_t gencnt, hpx_addr_t rsync);


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
/// @param gencnt The counter to wait for.
/// @param    gen The generation to wait for.
///
/// @returns HPX_SUCCESS or an error code.
hpx_status_t hpx_lco_gencount_wait(hpx_addr_t gencnt, unsigned long gen);


/// Perform a commutative-associative reduction.
///
/// This is similar to an ALLREDUCE. It is statically sized at creation time,
/// and is used cyclically. There is no non-blocking hpx_lco_get() operation, so
/// users should wait to call it until they know that they need it.
/// @{


/// The commutative-associative operation type.
///
/// Common operations would be min, max, +, *, etc.
typedef void (*hpx_commutative_associative_op_t)(void *lhs, const void *rhs);


/// Allocate a new reduction LCO.
///
/// The reduction is allocated in reduce-mode, i.e., it expects @p participants
/// to call the hpx_lco_set() operation as the first phase of operation.
///
/// @param participants The static number of participants in the reduction.
/// @param size         The size of the data being reduced.
/// @param op           The commutative-associative operation we're performing.
/// @param initializer  An initialization function for the data, this is used to
///                     initialize the data in every epoch.
hpx_addr_t hpx_lco_reduce_new(size_t participants, size_t size,
                              hpx_commutative_associative_op_t op,
                              void (*initializer)(void *));
/// @}

#endif
