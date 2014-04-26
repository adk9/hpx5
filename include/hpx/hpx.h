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

#include <assert.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "hpx/attributes.h"

/// Extern HPX macros
/// @{
typedef enum {
  HPX_ERROR         = -1,
  HPX_SUCCESS       = 0,
  HPX_RESEND        = 1,
  HPX_LCO_EXCEPTION = 2,                        //
  HPX_USER          = 127
} hpx_status_t;
/// @}


/// ----------------------------------------------------------------------------
/// An HPX global address.
///
/// HPX manages global addresses on a per-block basis. Blocks are allocated
/// during hpx_alloc or hpx_global_alloc, and can be up to 2^32 bytes large.
/// ----------------------------------------------------------------------------
typedef struct {
  uint64_t offset;                              // absolute offset
  uint32_t base_id;                             // base block id
  uint32_t block_bytes;                         // number of bytes per block
} hpx_addr_t;

#define HPX_ADDR_INIT(OFFSET, BASE, BYTES)       \
  {                                              \
    .offset = (OFFSET),                          \
    .base_id = (BASE),                           \
    .block_bytes = (BYTES)                       \
    }

static inline hpx_addr_t hpx_addr_init(uint64_t offset, uint32_t base,
                                       uint32_t bytes) {
  assert(bytes != 0);
  hpx_addr_t addr = HPX_ADDR_INIT(offset, base, bytes);
  return addr;
}

extern const hpx_addr_t HPX_NULL;
extern hpx_addr_t HPX_HERE;
hpx_addr_t HPX_THERE(int i);


/// ----------------------------------------------------------------------------
/// Allocate distributed global memory.
///
/// This is not a collective operation, the returned address is returned only to
/// the calling thread, and must either be written into already-allocated global
/// memory, or sent via a parcel, for anyone else to address the allocation.
///
/// In UPC-land, the returned global address would have the following
/// distribution.
///
/// shared [bytes] char foo[n * bytes];
/// ----------------------------------------------------------------------------
hpx_addr_t hpx_global_alloc(size_t n, uint32_t bytes);


/// ----------------------------------------------------------------------------
/// Allocate local global memory.
///
/// The returned address is local to the calling locality, and not distributed,
/// but can be used from any locality.
/// ----------------------------------------------------------------------------
hpx_addr_t hpx_alloc(size_t bytes);


/// ----------------------------------------------------------------------------
/// Free a global allocation.
/// ----------------------------------------------------------------------------
void hpx_global_free(hpx_addr_t addr);


/// ----------------------------------------------------------------------------
/// returns true if the addresses are equal
/// ----------------------------------------------------------------------------
bool hpx_addr_eq(const hpx_addr_t lhs, const hpx_addr_t rhs);


/// ----------------------------------------------------------------------------
/// global address arithmetic
/// ----------------------------------------------------------------------------
hpx_addr_t hpx_addr_add(const hpx_addr_t addr, int bytes);


/// ----------------------------------------------------------------------------
/// Performs address translation.
///
/// This will try and perform a global-to-local translation on the global @p
/// addr, and set @p out to the local address if it is successful. If @p local
/// is NULL, then this only performs address translation. If the address is not
/// local, or @p is not NULL and the pin fails, this will return false,
/// otherwise it will return true.
///
/// Successful pinning operations must be matched with an unpin operation, if
/// the underlying data is ever to be moved.
///
/// @param       addr - the global address
/// @param[out] local - the pinned local address
/// @returns          - { true; if @p addr is local and @p local is NULL
///                       true; if @p addr is local and @p is not NULL and pin
///                             is successful
///                       false; if @p is not local
///                       false; if @p is local and @local is not NULL and pin
///                              fails
/// ----------------------------------------------------------------------------
bool hpx_addr_try_pin(const hpx_addr_t addr, void **local);


/// ----------------------------------------------------------------------------
/// Allows the address to be remapped.
/// ----------------------------------------------------------------------------
void hpx_addr_unpin(const hpx_addr_t addr);

/// ----------------------------------------------------------------------------
/// Change the locality-affinity of a global distributed memory address.
///
/// This operation is only valid in the AGAS GAS mode. For PGAS, it is effectively
/// a no-op.
///
/// @param         src - the source address to move
/// @param         dst - the address pointing to the target locality to move the
///                      source address @p src to move to.
/// @param[out]    lco - LCO object to check the completion of move.
/// ----------------------------------------------------------------------------
void hpx_move(hpx_addr_t src, hpx_addr_t dst, hpx_addr_t lco);


typedef enum {
  HPX_GAS_DEFAULT = 0,
  HPX_GAS_NOGLOBAL,
  HPX_GAS_PGAS,
  HPX_GAS_AGAS,
  HPX_GAS_PGAS_SWITCH,
  HPX_GAS_AGAS_SWITCH
} hpx_gas_t;

typedef enum {
  HPX_TRANSPORT_DEFAULT = 0,
  HPX_TRANSPORT_SMP,
  HPX_TRANSPORT_MPI,
  HPX_TRANSPORT_PORTALS,
  HPX_TRANSPORT_PHOTON
} hpx_transport_t;

typedef enum {
  HPX_BOOT_DEFAULT = 0,
  HPX_BOOT_SMP,
  HPX_BOOT_MPI,
  HPX_BOOT_PMI
} hpx_boot_t;

typedef enum {
  HPX_WAIT_NONE = 0,
  HPX_WAIT
} hpx_wait_t;

typedef enum {
  HPX_LOCALITY_NONE = -2,
  HPX_LOCALITY_ALL = -1
} hpx_locality_t;

/// ----------------------------------------------------------------------------
/// The HPX configuration type.
/// ----------------------------------------------------------------------------
typedef struct {
  int                 cores;                  // number of cores to run on
  int               threads;                  // number of HPX scheduler threads
  unsigned int  backoff_max;                  // upper bound for backoff
  int           stack_bytes;                  // minimum stack size in bytes
  hpx_gas_t             gas;                  // GAS algorithm
  hpx_transport_t transport;                  // transport to use
  hpx_wait_t           wait;                  // when to wait for a debugger
  hpx_locality_t    wait_at;                  // locality to wait on
} hpx_config_t;

#define HPX_CONFIG_DEFAULTS {                   \
    .cores = 0,                                 \
    .threads = 0,                               \
    .backoff_max = 1024,                        \
    .stack_bytes = 65536,                       \
    .gas = HPX_GAS_PGAS,                        \
    .transport = HPX_TRANSPORT_DEFAULT,         \
    .wait = HPX_WAIT_NONE,                      \
    .wait_at = HPX_LOCALITY_NONE                \
    }

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


typedef uintptr_t hpx_action_t;
typedef int (*hpx_action_handler_t)(void *);
extern hpx_action_t HPX_ACTION_NULL;

/// ----------------------------------------------------------------------------
/// Should be called by the main native thread only, between the execution of
/// hpx_init() and hpx_run(). Should not be called from an HPX lightweight
/// thread.
///
/// @param   id - a unique string name for the action
/// @param func - the local function pointer to associate with the action
/// @returns    - a key to be used for the action when needed
/// ----------------------------------------------------------------------------
hpx_action_t hpx_register_action(const char *id, hpx_action_handler_t func);


/// ----------------------------------------------------------------------------
/// Simplify the registration interface slightly.
/// ----------------------------------------------------------------------------
#define HPX_STR(l) #l
#define HPX_REGISTER_ACTION(f) \
  hpx_register_action(HPX_STR(_hpx##f), (hpx_action_handler_t)f)


/// ----------------------------------------------------------------------------
/// This finalizes action registration, starts up any scheduler and native
/// threads that need to run, and transfers all control into the HPX scheduler,
/// beginning execution in @p entry. Returns the hpx_shutdown() code.
///
/// The @p entry paramter may be HPX_ACTION_NULL, in which case this entire
/// scheduler instance is running, waiting for a successful inter-locality steal
/// operation (if that is implemented) or a network parcel.
/// ----------------------------------------------------------------------------
int hpx_run(hpx_action_t entry, const void *args, unsigned size);


/// ----------------------------------------------------------------------------
/// This causes the main native thread to return the @p code from hpx_run(). The
/// returned thread is executing the original native thread, and all
/// supplementary scheduler threads and network will have been shutdown, and any
/// library resources will have been cleaned up.
///
/// This call is cooperative and synchronous, so it may not return if there are
/// misbehaving HPX lightweight threads.
/// ----------------------------------------------------------------------------
void hpx_shutdown(int code) HPX_NORETURN;


/// ----------------------------------------------------------------------------
/// This causes the main native thread to return the @p code from hpx_run(). The
/// returned thread is executing the original native thread, and all
/// supplementary scheduler threads an network will ahve been shutdown.
///
/// This is not cooperative, and will not clean up any resources. Furthermore,
/// the state of the system after the return is not well defined. The
/// application's main native thread should only rely on the async-safe
/// interface provided in signal(7).
/// ----------------------------------------------------------------------------
void hpx_abort(void) HPX_NORETURN;


/// ----------------------------------------------------------------------------
/// HPX topology interface
/// ----------------------------------------------------------------------------
hpx_locality_t hpx_get_my_rank(void);
int hpx_get_num_ranks(void);
int hpx_get_num_threads(void);
int hpx_get_my_thread_id(void);

#define HPX_LOCALITY_ID hpx_get_my_rank()
#define HPX_LOCALITIES hpx_get_num_ranks()
#define HPX_THREAD_ID hpx_get_my_thread_id()
#define HPX_THREADS hpx_get_num_threads()


/// ----------------------------------------------------------------------------
/// HPX thread interface.
///
/// HPX threads are spawned as a result of hpx_parcel_send{_sync}(). The may
/// return values to their LCO continuations using this hpx_thread_exit() call,
/// which terminates the thread's execution.
/// ----------------------------------------------------------------------------
hpx_addr_t hpx_thread_current_target(void);
hpx_addr_t hpx_thread_current_cont(void);
uint32_t   hpx_thread_current_args_size(void);


/// ----------------------------------------------------------------------------
/// Generates a consecutive new ID for a thread.
///
/// The first time this is called in a lightweight thread, it assigns the thread
/// the next available ID. Each time it's called after that it returns that same
/// id.
///
/// @returns < 0 if there is an error, otherwise a unique, compact id for the
///          calling thread
/// ----------------------------------------------------------------------------
int hpx_thread_get_tls_id(void);


/// ----------------------------------------------------------------------------
/// Finishes the current thread's execution, sending @p value to the thread's
/// continuation address.
/// ----------------------------------------------------------------------------
void hpx_thread_continue(size_t size, const void *value)
  HPX_NORETURN;
#define HPX_THREAD_CONTINUE(v) hpx_thread_continue(sizeof(v), &v)


/// ----------------------------------------------------------------------------
/// Finishes the current thread's execution, sending @p value to the thread's
/// continuation address.
///
/// This version gives the application a chance to cleanup for instance, to free
/// the value. After dealing with the continued data, it will run cleanup(env).
/// ----------------------------------------------------------------------------
void hpx_thread_continue_cleanup(size_t size, const void *value,
                                 void (*cleanup)(void*), void *env)
  HPX_NORETURN;


/// ----------------------------------------------------------------------------
/// Finish the current thread's execution.
///
/// The behavior of this call depends on the @p status parameter, and is
/// equivalent to returning @p status from the action.
///
///       HPX_SUCCESS: Normal termination, send a parcel with 0-sized data to
///                    the thread's continuation address.
///
///         HPX_ERROR: Abnormal termination. Terminates execution.
///
///        HPX_RESEND: Terminate execution, and resend the thread's parcel (NOT
///                    the continuation parcel). This can be used for
///                    application-level forwarding when hpx_addr_try_pin()
///                    fails.
///
/// HPX_LCO_EXCEPTION: Continue an exception to the continuation address.
/// ----------------------------------------------------------------------------
void hpx_thread_exit(int status)
  HPX_NORETURN;


/// ----------------------------------------------------------------------------
/// LCO's are local control objects. All LCOs support the "set" and "delete"
/// action interface, while subclasses, might provide more specific interfaces.
/// ----------------------------------------------------------------------------
void hpx_lco_delete(hpx_addr_t lco, hpx_addr_t sync);


/// ----------------------------------------------------------------------------
/// Set an LCO with a status.
/// ----------------------------------------------------------------------------
void hpx_lco_set_status(hpx_addr_t lco, const void *value, int size,
                        hpx_status_t status, hpx_addr_t sync);


/// ----------------------------------------------------------------------------
/// Set an LCO, optionally with data.
/// ----------------------------------------------------------------------------
void hpx_lco_set(hpx_addr_t lco, const void *value, int size, hpx_addr_t sync);


/// ----------------------------------------------------------------------------
/// Perform a wait operation.
///
/// The LCO blocks the caller until an LCO set operation signals the LCO. Each
/// LCO type has its own semantics for the state under which this occurs.
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
/// transitions from 0 to 1. This is asynchronous, if the caller needs
/// synchronous operation then they should pass an LCO address as @p sync, and
/// wait on it.
///
/// @param sema - the global address of a semaphore
/// @param sync - the global address of an LCO, may be HPX_NULL
/// ----------------------------------------------------------------------------
void hpx_lco_sema_v(hpx_addr_t sema, hpx_addr_t sync);


/// ----------------------------------------------------------------------------
/// Standard semaphore P operation.
///
/// Attempts to decrement the count in the semaphore, blocks if the count is 0.
///
/// @param sema - the global address of a semaphore
/// ----------------------------------------------------------------------------
void hpx_lco_sema_p(hpx_addr_t sema);


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
/// HPX parcel interface.
///
/// Parcels are the HPX message type.
/// ----------------------------------------------------------------------------
typedef struct hpx_parcel hpx_parcel_t;
hpx_parcel_t *hpx_parcel_acquire(size_t payload_bytes) HPX_MALLOC;
void hpx_parcel_release(hpx_parcel_t *p) HPX_NON_NULL(1);
void hpx_parcel_send(hpx_parcel_t *p) HPX_NON_NULL(1); // implies release
hpx_action_t hpx_parcel_get_action(const hpx_parcel_t *p) HPX_NON_NULL(1);
hpx_addr_t hpx_parcel_get_target(const hpx_parcel_t *p) HPX_NON_NULL(1);
hpx_addr_t hpx_parcel_get_cont(const hpx_parcel_t *p) HPX_NON_NULL(1);
void *hpx_parcel_get_data(hpx_parcel_t *p) HPX_NON_NULL(1);
void hpx_parcel_set_action(hpx_parcel_t *p, const hpx_action_t action)
  HPX_NON_NULL(1);
void hpx_parcel_set_target(hpx_parcel_t *p, const hpx_addr_t addr)
  HPX_NON_NULL(1);
void hpx_parcel_set_cont(hpx_parcel_t *p, const hpx_addr_t lco)
  HPX_NON_NULL(1);
void hpx_parcel_set_data(hpx_parcel_t *p, const void *data, int size)
  HPX_NON_NULL(1);


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
/// HPX collective operations.
///
/// This is a parallel call interface that performs an @p action on @p args at
/// all available localities. The output values are not returned, but the
/// completion of the broadcast operation can be tracked through the @p lco LCO.
/// ----------------------------------------------------------------------------
int hpx_bcast(hpx_action_t action, const void *args, size_t len, hpx_addr_t lco);


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
double hpx_time_us(hpx_time_t from, hpx_time_t to);
double hpx_time_ms(hpx_time_t from, hpx_time_t to);
double hpx_time_elapsed_us(hpx_time_t from);
double hpx_time_elapsed_ms(hpx_time_t from);


const char* hpx_get_network_id(void);

#ifdef __cplusplus
}
#endif

#endif // HPX_H
