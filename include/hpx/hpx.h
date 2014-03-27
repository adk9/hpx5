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
#ifndef HPX_H
#define HPX_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "hpx/attributes.h"

/// External HPX typedefs
/// @{
typedef uintptr_t hpx_action_t;
typedef int (*hpx_action_handler_t)(void *);
/// @}

extern hpx_action_t HPX_ACTION_NULL;

/// Extern HPX macros
/// @{
#define HPX_SUCCESS 0
#define HPX_ERROR -1
/// @}


/// An HPX global address. At release, this could be encapsulated in a more
/// opaque structure, like a 128 bit integer, or a 16 byte character array. This
/// would protect the API, and compiled application code, from address algorithm
/// changes.
typedef struct {
  uint64_t offset;                              // absolute offset
  uint32_t id;                                  // unique allocation identifier
  uint32_t block_bytes;                         // number of bytes per block
} hpx_addr_t;

#define HPX_ADDR_INIT { \
    .offset = 0,        \
    .id = 0,            \
    .block_bytes = 0    \
}

extern const hpx_addr_t HPX_NULL;
//extern const hpx_addr_t HPX_ANYWHERE;
extern hpx_addr_t HPX_HERE;
hpx_addr_t HPX_THERE(int i);
int HPX_WHERE(hpx_addr_t addr);

/// Allocate distributed global memory.
///
/// This is not a collective operation, the returned address is returned only to
/// the calling thread, and must either be written into already-allocated global
/// memory, or sent via a parcel, for anyone else to address the allocation.
///
/// In UPC-land, the returned global address would have the following
/// distribution.
///
/// shared [block_size] T[n]; where sizeof(T) == bytes
///
hpx_addr_t hpx_global_alloc(size_t n, size_t bytes, size_t block_size,
                             size_t alignment);

/// Allocate local global memory.
///
/// The returned address is local to the calling rank, and not distributed, but
/// can be used from any rank.
hpx_addr_t hpx_alloc(size_t n, size_t bytes, size_t alignment);

/// Free a global allocation.
void hpx_global_free(hpx_addr_t addr);


/// returns true if the addresses are equal
bool hpx_addr_eq(const hpx_addr_t lhs, const hpx_addr_t rhs);


/// global address arithmetic
hpx_addr_t hpx_addr_add(const hpx_addr_t addr, int bytes);


/// Performs address translation.
///
/// This will try and perform a global-to-local translation on the global @p
/// addr, and set @p out to the local address if it is successful. If @p local
/// is NULL, then this only performs address translation. If the address is not
/// local, or @p is not NULL and the pin fails, this will return false,
/// otherwise it will return true.
///
/// @param       addr - the global address
/// @param[out] local - the pinned local address
/// @returns          - { true; if @p addr is local and @p local is NULL
///                       true; if @p addr is local and @p is not NULL and pin
///                             is successful
///                       false; if @p is not local
///                       flase; if @p is local and @local is not NULL and pin
///                              fails
bool hpx_addr_try_pin(const hpx_addr_t addr, void **local);


/// Allows the address to be remapped.
void hpx_addr_unpin(const hpx_addr_t addr);


/// The HPX configuration type. This can be allocated and set manually by the
/// application, or used with the provided argp functionality to extract the
/// configuration using a set of HPX-standard command line arguments.
///
/// see <argp.h>
typedef struct {
  int cores;                                  // number of cores to use
  int threads;                                // number of HPX scheduler threads
  int stack_bytes;                            // minimum stack size in bytes
} hpx_config_t;


/// ----------------------------------------------------------------------------
/// HPX system interface.
///
/// hpx_init() initializes the scheduler, network, and locality
///
/// hpx_register_action() register a user-level action with the runtime.
///
/// hpx_run() is called from the native thread after hpx_init() and action
/// registration is complete, in order
///
/// hpx_abort() is called from an HPX lightweight thread to terminate scheduler
/// execution asynchronously
///
/// hpx_shutdown() is called from an HPX lightweight thread to terminate
/// scheduler execution
/// ----------------------------------------------------------------------------
int hpx_init(const hpx_config_t *config);


/// Should be called by the main native thread only, between the execution of
/// hpx_init() and hpx_run(). Should not be called from an HPX lightweight
/// thread.
///
/// @param   id - a unique string name for the action
/// @param func - the local function pointer to associate with the action
/// @returns    - a key to be used for the action when needed
hpx_action_t hpx_register_action(const char *id, hpx_action_handler_t func);


/// This finalizes action registration, starts up any scheduler and native
/// threads that need to run, and transfers all control into the HPX scheduler,
/// beginning execution in @p entry. Returns the hpx_shutdown() code.
///
/// The @p entry paramter may be HPX_ACTION_NULL, in which case this entire
/// scheduler instance is running, waiting for a successful inter-locality steal
/// operation (if that is implemented) or a network parcel.
int hpx_run(hpx_action_t entry, const void *args, unsigned size);


/// This causes the main native thread to return the @p code from hpx_run(). The
/// returned thread is executing the original native thread, and all
/// supplementary scheduler threads and network will have been shutdown, and any
/// library resources will have been cleaned up.
///
/// This call is cooperative and synchronous, so it may not return if there are
/// misbehaving HPX lightweight threads.
void hpx_shutdown(int code) HPX_NORETURN;


/// This causes the main native thread to return the @p code from hpx_run(). The
/// returned thread is executing the original native thread, and all
/// supplementary scheduler threads an network will ahve been shutdown.
///
/// This is not cooperative, and will not clean up any resources. Furthermore,
/// the state of the system after the return is not well defined. The
/// application's main native thread should only rely on the async-safe
/// interface provided in signal(7).
void hpx_abort(int code) HPX_NORETURN;


/// HPX system interface
int hpx_get_my_rank(void);
int hpx_get_num_ranks(void);
int hpx_get_num_threads(void);
int hpx_get_my_thread_id(void);


/// ----------------------------------------------------------------------------
/// HPX thread interface.
///
/// HPX threads are spawned as a result of hpx_parcel_send{_sync}(). The may
/// return values to their LCO continuations using this hpx_thread_exit() call,
/// which terminates the thread's execution.
/// ----------------------------------------------------------------------------
void hpx_thread_exit(int status, const void *value, size_t size) HPX_NORETURN;
const hpx_addr_t hpx_thread_current_target(void);
const hpx_addr_t hpx_thread_current_cont(void);


/// ----------------------------------------------------------------------------
/// LCO's are local control objects.
///
/// LCO's are used for data-driven execution.
///
/// The hpx_lco_get{_all}() operations will block the calling thread until the
/// target LCO has been set using an hpx_lco_set() that has the effect of moving
/// the LCO to set.
/// ----------------------------------------------------------------------------


/// ----------------------------------------------------------------------------
/// Semaphores are builtin LCOs that represent resource usage.
/// ----------------------------------------------------------------------------
hpx_addr_t hpx_sema_new(unsigned init);
void hpx_sema_delete(hpx_addr_t sema);
void hpx_sema_v(hpx_addr_t sema);        // increments the semaphore, async
void hpx_sema_p(hpx_addr_t sema);        // decrement the semaphore, blocks if 0


/// ----------------------------------------------------------------------------
/// Futures are builtin LCOs that represent values returned from asynchronous
/// computation.
/// ----------------------------------------------------------------------------
hpx_addr_t hpx_future_new(int size);
hpx_addr_t hpx_future_new_array(int n, int size, int block_size);
void hpx_future_delete(hpx_addr_t future);
void hpx_future_get(hpx_addr_t future, void *value, int size);
void hpx_future_get_all(unsigned n, hpx_addr_t future[], void *values[], const int sizes[]);
void hpx_future_set(hpx_addr_t future, const void *value, int size);

/// ----------------------------------------------------------------------------
/// Counter LCOs represent monotonically increasing values. The get
/// operation blocks the calling thread until the "counted" value
/// reaches the limit.
/// ----------------------------------------------------------------------------
hpx_addr_t hpx_lco_counter_new(uint64_t limit);
void hpx_lco_counter_delete(hpx_addr_t counter);
void hpx_lco_counter_wait(hpx_addr_t counter);
void hpx_lco_counter_incr(hpx_addr_t counter, const uint64_t amount);


/// ----------------------------------------------------------------------------
/// HPX parcel interface.
///
/// Parcels are the HPX message type.
/// ----------------------------------------------------------------------------
typedef struct hpx_parcel hpx_parcel_t;

/// allocate a parcel with the given payload size
hpx_parcel_t *hpx_parcel_acquire(size_t payload_bytes) HPX_MALLOC;

/// release the parcel, either directly, or by sending it
void hpx_parcel_release(hpx_parcel_t *p) HPX_NON_NULL(1);
void hpx_parcel_send(hpx_parcel_t *p) HPX_NON_NULL(1);

void hpx_parcel_set_action(hpx_parcel_t *p, const hpx_action_t action) HPX_NON_NULL(1);
void hpx_parcel_set_target(hpx_parcel_t *p, const hpx_addr_t addr) HPX_NON_NULL(1);
void hpx_parcel_set_cont(hpx_parcel_t *p, const hpx_addr_t lco) HPX_NON_NULL(1);
void hpx_parcel_set_data(hpx_parcel_t *p, const void *data, int size) HPX_NON_NULL(1);

hpx_action_t hpx_parcel_get_action(const hpx_parcel_t *p) HPX_NON_NULL(1);
hpx_addr_t hpx_parcel_get_target(const hpx_parcel_t *p) HPX_NON_NULL(1);
hpx_addr_t hpx_parcel_get_cont(const hpx_parcel_t *p) HPX_NON_NULL(1);
void *hpx_parcel_get_data(hpx_parcel_t *p) HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// HPX call interface.
///
/// Performs @p action on @p args at @p addr, and sets @p result with the
/// resulting value. HPX_NULL is value for @p result, in which case no return
/// value is generated.
///
/// @param   addr - the address that defines where the action is executed
/// @param action - the action to perform
/// @param   args - the argument data for @p action
/// @param    len - the length of @p args
/// @param result - an address of an LCO to trigger with the result
/// ----------------------------------------------------------------------------
int hpx_call(hpx_addr_t addr, hpx_action_t action, const void *args,
             size_t len, hpx_addr_t result);


/// ----------------------------------------------------------------------------
/// HPX high-resolution timer interface
/// ----------------------------------------------------------------------------
#if defined(__linux__)
#include <time.h>
typedef struct timespec hpx_time_t;
#define HPX_TIME_INIT {0}
#elif defined(__APPLE__)
#include <stdint.h>
typedef uint64_t hpx_time_t;
#define HPX_TIME_INIT (0)
#endif


hpx_time_t hpx_time_now(void);
double hpx_time_elapsed_us(hpx_time_t);
double hpx_time_elapsed_ms(hpx_time_t);


const char* hpx_get_network_id(void);

#ifdef __cplusplus
}
#endif

#endif // HPX_H
