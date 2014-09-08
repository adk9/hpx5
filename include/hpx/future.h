// Specifies state of a future as returned by wait_for and wait_until functions
// of future.
enum class future_status {
  // The shared state is ready.
  ready,
  // The shared state did not become ready before specified timeout duration
  // has passed.
  timeout,
  // The shared state contains a deferred function, so the result will be
  // computed only when explicitly requested.
  deferred
};

typedef enum {HPX_SET, HPX_UNSET} hpx_set_t;

/// FT_FREE is set to true if there are pre-allocated future description
#define FT_FREE     0x00
/// FT_EMPTY is set if the future container size is 0, false otherwise
#define FT_EMPTY    0x01
/// FT_FULL is set if the future container size is full, false otherwise
#define FT_FULL     0x03
/// Async is when the action is defined with the future, to be executed
/// asynchronously. The creater of the asynchronous operation can then use
/// a variety of methods to query waitfor, or extract a value from future.
/// These may block if the asynchronous operation has not yet provided a
/// value
#define FT_ASYNCH   0x05
/// Wait has a waiter already
#define FT_WAIT     0x09
/// Waits for the result. Gets set if it is not available for the specific
/// timeout duration.
#define FT_WAITFORA 0x0D
/// Waits for the result, gets set if result is not available until specified
/// time pount has been reached
#define FT_WAITUNTILA 0x0E
/// This state is set to true if *this refers to a shared state otherwise
/// false.
#define FT_SHARED   0x10

/// Create a future
///
/// Although globally accessible, this future will be created at the
/// calling locality.
///
/// @param size                 The number of bytes e
/// @returns                    The global address of the future
hpx_addr_t hpx_lco_newfuture_new(size_t size);

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
hpx_addr_t hpx_lco_newfuture_new_all(int num_participants, size_t size_per_participant);

/// Get the address of a future in an array
///
/// @param base The base address for the array of futures
/// @param   id The index into the array of futures
hpx_addr_t hpx_lco_newfuture_at(hpx_addr_t base, int id);

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
hpx_status_t hpx_lco_newfuture_setat(hpx_addr_t future,  int id, size_t size, void *data,
                                  hpx_addr_t lsync_lco, hpx_addr_t rsync_lco);

/// Reset a future to empty
///
/// Set future @p future to be empty, enabling it to be set again,
/// and notifying any threads waiting for it to be emptied
/// @param future    The global address of the future
/// @param  id       The index in array of futures
/// @param rsync_lco An LCO to be set when the newfuture has been successfully reset
hpx_status_t hpx_lco_newfuture_emptyat(hpx_addr_t newfuture,  int id, hpx_addr_t rsync_lco);

/// Get a future
///
/// Get the value of the future, waiting if necessary.
///
/// @param future The address of the future
/// @param   size The amount of data to get, in bytes
/// @param  id    The index in array of futures
/// @param value  Address of data value
/// @returns      The address of the value of the future
hpx_addr_t hpx_lco_newfuture_getat(hpx_addr_t future, int id, size_t size, void *value);

/// Get the values of multiple futures simultaneously
///
/// Get the values of @p num futures, waiting until all values have been
/// received.
///
/// @param[in]     num The number of futures to wait on
/// @param[in] futures An array of the addresses of the futures
/// @param[in]   sizes An array of the amounts of data to get, in bytes
/// @param[out] values The addresses of the values of the futures
void hpx_lco_newfuture_get_all(size_t num, hpx_addr_t *futures, size_t *size,
                        void *values);

/// Wait on a future
///
/// Wait for a future to be set, or to become available to be set
///
/// @param future The address of the future
/// @param     id The index in array of futures
/// @param    set Wait until future is set or reset?
void hpx_lco_newfuture_waitat(hpx_addr_t future, int id, hpx_set_t set);

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
hpx_status_t hpx_lco_newfuture_waitat_for(hpx_addr_t future, int id, hpx_set_t set, hpx_time_t for_time);

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
hpx_status_t hpx_lco_newfuture_waitat_until(hpx_addr_t future, int id, hpx_set_t set, hpx_time_t for_time);

/// Wait on a multiple futures
///
/// Wait for a @p num futures to be set (or unset)
///
/// @param     num The number of futures to wait on
/// @param futures An array of addresses of the future
/// @param     set Wait until future is set or reset?
void hpx_lco_newfuture_wait_all(size_t num, hpx_addr_t *newfutures, hpx_set_t set);

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
hpx_status_t hpx_lco_newfuture_wait_all_for(size_t num, hpx_addr_t *newfutures, 
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
hpx_status_t hpx_lco_newfuture_wait_all_until(size_t num, hpx_addr_t *newfutures, 
				       hpx_set_t set, hpx_time_t time);

/// Free a future
///
/// @param future The address of the future
void hpx_lco_newfuture_free(hpx_addr_t newfuture);

/// Free multiple futures
///
/// @param     num The number of futures to free
/// @param futures An array of the addresses of the newfutures
void hpx_lco_newfuture_free_all(size_t num, hpx_addr_t *newfutures);

/// Check to see if a future is shared
///
/// If a future is shared it can be waited on or set
///
/// @returns true if the future may given as an arugument to hpx_lco_future_set(), 
///          hpx_lco_future_get(), or hpx_lco_future_wait(), etc.
///          false otherwise
bool hpx_lco_newfuture_is_shared();

