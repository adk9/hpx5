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

typedef struct  {
  struct {
    uintptr_t addr;
    uint64_t size;
    uint64_t offset;
    struct {
      uint64_t key0;
      uint64_t key1;
    } priv;
    // i.e. struct photon_buffer_priv_t priv;
  } buffer;
  // i.e. struct photon_buffer_t buffer;
  // of interest are buffer.priv and buffer.addr
  int count; // number of futures in this array
  int base_rank;
  int size_per; // size per future in bytes
  int id; // index
  // actual address is buffer.addr + (id % HPX_NUM_LOCALITIES) * (sizeof(newfuture_t) + size_per) 
} hpx_newfuture_t;

/// Create a future
///
/// Although globally accessible, this future will be created at the
/// calling locality.
///
/// @param size                 The number of bytes
/// @returns                    The global address of the future
hpx_newfuture_t *hpx_lco_newfuture_new(size_t size);

/// Creates a shared future
/// 
/// Shared futures can be accessed mutiple times. They must be manually emptied
/// with hpx_lco_newfuture_emptyat()
/// @param size                 The number of bytes
/// @returns                    The global address of the future
hpx_newfuture_t *hpx_lco_newfuture_shared_new(size_t size);

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
hpx_newfuture_t *hpx_lco_newfuture_new_all(int num_participants, size_t size_per_participant);

/// Creates an array of shared futures
/// 
/// Shared futures can be accessed mutiple times
/// @param num_participants The number of shared futures to allocate
/// @param size             The number of bytes per shared future
/// @returns                The global address of the future
hpx_newfuture_t *hpx_lco_newfuture_shared_new_all(int num_participants, size_t size);

/// Get the address of a future in an array
///
/// @param base The base address for the array of futures
/// @param   id The index into the array of futures
hpx_newfuture_t *hpx_lco_newfuture_at(hpx_newfuture_t *base, int id);

/// Set a future
///
/// Set future @p future with @p data of @p size bytes. Set @p lsync_lco
/// when the data is sent to the future, and set @p rsync_lco when the future is
/// set at the (possibly) remote locality.
/// @param future    The global address of the future
/// @param  id       The index in array of futures
/// @param   size    The size of @p data in bytes
/// @param   data    The data the future will be set with
/// @param lsync_lco An LCO to be set when the data is sent to the future
/// @param rsync_lco An LCO to be set when the future has been successfully set
void hpx_lco_newfuture_setat(hpx_newfuture_t *future,  int id, size_t size, void *data,
                                  hpx_addr_t lsync_lco, hpx_addr_t rsync_lco);

/// Reset a future to empty
///
/// Set future @p future to be empty, enabling it to be set again,
/// and notifying any threads waiting for it to be emptied
/// @param future    The global address of the future
/// @param  id       The index in array of futures
/// @param rsync_lco An LCO to be set when the newfuture has been successfully reset
void hpx_lco_newfuture_emptyat(hpx_newfuture_t *newfuture,  int id, hpx_addr_t rsync_lco);

/// Get a future
///
/// Get the value of the future, waiting if necessary.
///
/// @param future The address of the future
/// @param   size The amount of data to get, in bytes
/// @param  id    The index in array of futures
/// @param value  Address of data value
/// @returns      Either HPX_SUCCESS or some error status
hpx_status_t hpx_lco_newfuture_getat(hpx_newfuture_t *future, int id, size_t size, void *value);

/// Get the values of multiple futures simultaneously
///
/// Get the values of @p num futures, waiting until all values have been
/// received.
///
/// @param[in]     num The number of futures to wait on
/// @param[in] futures An array of the addresses of the futures
/// @param[in]   sizes The amount of data to get at each location, in bytes
/// @param[out] values Array of the addresses of the values of the futures
void hpx_lco_newfuture_get_all(size_t num, hpx_newfuture_t *futures, size_t size,
			       void *values[]);

/// Wait on a future
///
/// Wait for a future to be set, or to become available to be set
///
/// @param future The address of the future
/// @param     id The index in array of futures
/// @param    set Wait until future is set or reset?
void hpx_lco_newfuture_waitat(hpx_newfuture_t *future, int id, hpx_set_t set);

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
hpx_status_t hpx_lco_newfuture_waitat_for(hpx_newfuture_t *future, int id, hpx_set_t set, hpx_time_t time);

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
hpx_status_t hpx_lco_newfuture_waitat_until(hpx_newfuture_t *future, int id, hpx_set_t set, hpx_time_t time);

/// Wait on a multiple futures
///
/// Wait for a @p num futures to be set (or unset)
///
/// @param     num The number of futures to wait on
/// @param futures An array of addresses of the future
/// @param     set Wait until future is set or reset?
void hpx_lco_newfuture_wait_all(size_t num, hpx_newfuture_t *newfutures, hpx_set_t set);

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
hpx_status_t hpx_lco_newfuture_wait_all_for(size_t num, hpx_newfuture_t *newfutures, 
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
hpx_status_t hpx_lco_newfuture_wait_all_until(size_t num, hpx_newfuture_t *newfutures, 
				       hpx_set_t set, hpx_time_t time);

/// Free a future
///
/// @param future The address of the future
void hpx_lco_newfuture_free(hpx_newfuture_t *future);

/// Free multiple futures
///
/// @param  num The number of futures in the array
/// @param base An array of the addresses of the newfutures
void hpx_lco_newfuture_free_all(int num, hpx_newfuture_t *base);

/// Check to see if a future is shared
///
/// If a future is shared it can be waited on or set
///
/// @param future A future
/// @returns      true if the future is a shared future, false otherwise
bool hpx_lco_newfuture_is_shared(hpx_newfuture_t *future);

/// Returns the rank at which the future is located (you might need to use hpx_lco_newfuture_at() to get the future).
int hpx_lco_newfuture_get_rank(hpx_newfuture_t *future);

#endif
