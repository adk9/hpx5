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
#ifndef HPX_FUTURE_H
#define HPX_FUTURE_H

#include "hpx/time.h"

// Specifies state of a future as returned by wait_for and wait_until functions
// of future.
typedef enum {
  // The shared state is ready.
  HPX_FUTURE_STATUS_READY,
  // The shared state did not become ready before specified timeout duration
  // has passed.
  HPX_FUTURE_STATUS_TIMEOUT,
  // The shared state contains a deferred function, so the result will be
  // computed only when explicitly requested.
  HPX_FUTURE_STATUS_DEFERRED
} hpx_future_status;

typedef enum {HPX_UNSET = 0x01, HPX_SET = 0x02} hpx_set_t;

/// Objects of hpx_netfuture_config_t are used to configure the netfutures
/// system at initialization time.
typedef struct {
  size_t max_size;         /// The maximum amount of total memory available
                           /// for all allocated netfutures.
  size_t max_array_number; /// The number of netfuture arrays
  size_t max_number;       /// The number of netfuture elements over all arrays
} hpx_netfuture_config_t;

#define HPX_NETFUTURE_CONFIG_DEFAULTS { \
    .max_size = 1024*1024*100,  \
    .max_array_number = 100,    \
    .max_number = 100000        \
}

typedef struct  {
  int table_index;
  uintptr_t base_offset;
  int size;
  int count;
  int index;
} hpx_netfuture_t;

/// Create a future
///
/// Although globally accessible, this future will be created at the
/// calling locality.
///
/// @param size                 The number of bytes
/// @returns                    The global address of the future
hpx_netfuture_t hpx_lco_netfuture_new(size_t size);

/// Creates a shared future
///
/// Shared futures can be accessed mutiple times. They must be manually emptied
/// with hpx_lco_netfuture_emptyat()
/// @param size                 The number of bytes
/// @returns                    The global address of the future
hpx_netfuture_t hpx_lco_netfuture_shared_new(size_t size);

/// Create an array of futures
///
/// Creates an array of @p num futures, each with @p num_participants
/// participants who will set this future before it is considered full,
/// each of whom will contribute @p size_per_participant bytes.
/// @p num_participants and @p size_per_participant should be an array
/// of size @p num.
///
/// The futures will be placed cyclically in globally memory.
///
/// @param num_participants     The number of participants who will set
///                             this future before it is full
/// @param size_per_participant The number of bytes each participant
///                             will be setting
/// @returns                    The global address of the future
hpx_netfuture_t hpx_lco_netfuture_new_all(int num_participants, size_t size_per_participant);

/// Creates an array of shared futures
///
/// Shared futures can be accessed mutiple times
/// @param num_participants The number of shared futures to allocate
/// @param size             The number of bytes per shared future
/// @returns                The global address of the future
hpx_netfuture_t hpx_lco_netfuture_shared_new_all(int num_participants, size_t size);

/// Get the address of a future in an array
///
/// @param base The base address for the array of futures
/// @param   id The index into the array of futures
hpx_netfuture_t hpx_lco_netfuture_at(hpx_netfuture_t base, int id);

/// Set a future
///
/// Set future @p future with @p data of @p size bytes. Set @p lsync_lco
/// when the data is sent to the future, and set @p rsync_lco when the future is
/// set at the (possibly) remote locality.
/// @param       future The global address of the future
/// @param           id The index in array of futures
/// @param         size The size of @p data in bytes
/// @param         data The data the future will be set with
/// @param        lsync An LCO to be set when the data is sent to the future.
void hpx_lco_netfuture_setat(hpx_netfuture_t future,  int id, size_t size, hpx_addr_t value,
                             hpx_addr_t lsync);

/// Reset a future to empty
///
/// Set future @p future to be empty, enabling it to be set again,
/// and notifying any threads waiting for it to be emptied
/// @param future    The global address of the future
/// @param  id       The index in array of futures
/// @param rsync_lco An LCO to be set when the netfuture has been successfully reset
void hpx_lco_netfuture_emptyat(hpx_netfuture_t netfuture,  int id, hpx_addr_t rsync_lco);

/// Get a future
///
/// Get the value of the future, waiting if necessary.
///
/// @param future The address of the future
/// @param   size The amount of data to get, in bytes
/// @param  id    The index in array of futures
/// @param value  Address of data value
/// @returns      Global memory with the value (or HPX_NULL on error)
hpx_addr_t hpx_lco_netfuture_getat(hpx_netfuture_t future, int id, size_t size);

/// Get the values of multiple futures simultaneously
///
/// Get the values of @p num futures, waiting until all values have been
/// received.
///
/// @param[in]     num The number of futures to wait on
/// @param[in] futures An array of the addresses of the futures
/// @param[in]   sizes The amount of data to get at each location, in bytes
/// @param[out] values Array of the addresses of the values of the futures
void hpx_lco_netfuture_get_all(size_t num, hpx_netfuture_t futures, size_t size,
                   void *values[]);

/// Wait on a future
///
/// Wait for a future to be set, or to become available to be set
///
/// @param future The address of the future
/// @param     id The index in array of futures
/// @param    set Wait until future is set or reset?
void hpx_lco_netfuture_waitat(hpx_netfuture_t future, int id, hpx_set_t set);

/// Wait on a future for a specified amount of time
///
/// Wait for a future to be set, or to become available to be set but
/// return early if future if a specified amount of time has expired
///
/// @param future The address of the future
/// @param    id The index in array of futures
/// @param   set Wait until future is set or reset?
/// @param  time Amount of time to wait for
/// @returns     HPX_SUCCESS if future is ready before time expires
hpx_status_t hpx_lco_netfuture_waitat_for(hpx_netfuture_t future, int id, hpx_set_t set, hpx_time_t time);

/// Wait on a future until a certain time
///
/// Wait for a future to be set, or to become available to be set but
/// return early if future if a specified time has been reached
///
/// @param future The address of the future
/// @param     id The index in array of futures
/// @param    set Wait until future is set or reset?
/// @param   time A set time to wait until
/// @returns      HPX_SUCCESS if future is ready before time expires
hpx_status_t hpx_lco_netfuture_waitat_until(hpx_netfuture_t future, int id, hpx_set_t set, hpx_time_t time);

/// Wait on a multiple futures
///
/// Wait for a @p num futures to be set (or unset)
///
/// @param     num The number of futures to wait on
/// @param futures An array of addresses of the future
/// @param     set Wait until future is set or reset?
void hpx_lco_netfuture_wait_all(size_t num, hpx_netfuture_t netfutures, hpx_set_t set);

/// Wait on a multiple futures for a set amount of time
///
/// Wait for a @p num futures to be set (or unset), or return early
/// if a set amount of time has passed
///
/// @param     num The number of futures to wait on
/// @param futures An array of addresses of the future
/// @param     set Wait until future is set or reset?
/// @param    time An amount of time to wait for before returning
/// @returns      HPX_SUCCESS if all futures are ready before time expires
hpx_status_t hpx_lco_netfuture_wait_all_for(size_t num, hpx_netfuture_t netfutures,
                     hpx_set_t set, hpx_time_t time);

/// Wait on a multiple futures until a set time
///
/// Wait for a @p num futures to be set (or unset), or return early
/// if a set time has been reached
///
/// @param     num The number of futures to wait on
/// @param futures An array of addresses of the future
/// @param     set Wait until future is set or reset?
/// @param    time A time to wait until
/// @returns      HPX_SUCCESS if all futures are ready before time expires
hpx_status_t hpx_lco_netfuture_wait_all_until(size_t num, hpx_netfuture_t netfutures,
                       hpx_set_t set, hpx_time_t time);

/// Free a future
///
/// @param future The address of the future
void hpx_lco_netfuture_free(hpx_netfuture_t future);

/// Free multiple futures
///
/// @param  num The number of futures in the array
/// @param base An array of the addresses of the netfutures
void hpx_lco_netfuture_free_all(int num, hpx_netfuture_t base);

/// Check to see if a future is shared
///
/// If a future is shared it can be waited on or set
///
/// @param future A future
/// @returns      true if the future is a shared future, false otherwise
bool hpx_lco_netfuture_is_shared(hpx_netfuture_t future);

/// Returns the rank at which the future is located (you might need to use hpx_lco_netfuture_at() to get the future).
int hpx_lco_netfuture_get_rank(hpx_netfuture_t future);

/// Must be called first, from the main action
/// @param cfg A configuration object, which may be NULL (which sets all values
///            to their defaults. If any field is set, all fields must be set.
hpx_status_t hpx_netfutures_init(hpx_netfuture_config_t *cfg);

/// Must be called from main action after last use of netfutures.
void hpx_netfutures_fini();

#endif
