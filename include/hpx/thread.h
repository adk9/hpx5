/*
 ====================================================================
  High Performance ParalleX Library (libhpx)

  Thread Functions
  network.h

  Copyright (c) 2013, Trustees of Indiana University
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/

#ifndef LIBHPX_THREAD_H_
#define LIBHPX_THREAD_H_

#ifndef __cplusplus
#include <stdbool.h>
#endif

#include "hpx/utils/list.h"                     /* hpx_list_t */

/** Forward declarations @{ */
struct hpx_future;
struct hpx_mctx_context;
struct hpx_kthread;
struct hpx_context;
struct hpx_map;
/** @} */

/** Typedefs @{ */
typedef uint64_t hpx_node_id_t;
typedef uint64_t hpx_thread_id_t;
typedef uint8_t  hpx_thread_state_t;
/** @} */

/**
 * Thread States
 */
enum hpx_thread_states {
  HPX_THREAD_STATE_UNDEFINED = 255,             /**< */
  HPX_THREAD_STATE_CREATE = 0,                  /**< */
  HPX_THREAD_STATE_INIT,                        /**< */
  HPX_THREAD_STATE_PENDING,                     /**< */
  HPX_THREAD_STATE_EXECUTING,                   /**< */
  HPX_THREAD_STATE_YIELD,                       /**< */
  HPX_THREAD_STATE_BLOCKED,                     /**< */
  HPX_THREAD_STATE_SUSPENDED,                   /**< */
  HPX_THREAD_STATE_TERMINATED                   /**< */
};

/**
 * Thread Options
 *
 * Values for a bitmask that contain option flags for HPX threads.
 *
 * Normally, thread stacks and machine context switching buffers are reused
 * after a thread is terminated.  Setting the thread to DETACHED will cause the
 * scheduler to immediately deallocate these data structures upon thread
 * termination.
 *
 * BOUND threads are pinned to the same kernel thread as their parent, as
 * opposed to being evenly distributed evenly across all kernel threads in a
 * round-robin fashion (the default).
 */
enum hpx_thread_options {
  HPX_THREAD_OPT_NONE               = 0,        /**< */
  HPX_THREAD_OPT_DETACHED           = 1,        /**< */
  HPX_THREAD_OPT_BOUND              = 2,        /**< */
  HPX_THREAD_OPT_SERVICE_CORELOCAL  = 4,        /**< */
  HPX_THREAD_OPT_SERVICE_COREGLOBAL = 8         /**< */
};

/**
 * An HPX function taking a single generic (void*) argument. This is the type
 * for HPX thread-entry functions.
 *
 * @param[in,out]
 */
typedef void (*hpx_func_t)(void *args);

/**
 * Predicate function type for _hpx_thread_wait.
 *
 * @param[in] lhs
 * @param[in] rhs
 * @returns
 */
typedef bool (*hpx_thread_wait_pred_t)(void *lhs, void *rhs);

/**
 * The reusable user-level thread data.
 */
struct hpx_thread_reusable {
  hpx_func_t               func;                /**< entry function  */
  void                    *args;                /**< arguments */
  void                     *stk;                /**< stack  */
  size_t                     ss;                /**< stack size  */
  struct hpx_mctx_context *mctx;                /**< machine context (jmpbuf) */
  void                    *wait;                /**< waiting future  */
  struct hpx_kthread       *kth;                /**< kernel thread */
};

/**
 * The user-level thread data.
 */
struct hpx_thread {
  struct hpx_context           *ctx;            /**< pointer to the context */
  hpx_node_id_t                 nid;            /**< node id */
  hpx_thread_id_t               tid;            /**< thread id */
  hpx_thread_state_t          state; /**< queuing state (@see hpx_thread_states) */
  uint16_t                     opts;            /**< thread option flags  */
  uint8_t                      skip;            /**< [reserved for future] */
  struct hpx_thread_reusable *reuse;            /**< @see hpx_thread_reusable */
  struct hpx_future          *f_ret;            /**< return value (if any) */
  struct hpx_thread         *parent;            /**< parent thread  */
  hpx_list_t               children;            /**< list of children */
};

/**
 * Creates a user-level thread in the local virtual environment.
 *
 * @todo Error return values need to be disambiguated.
 *
 * @param[in] ctx     - xpi_context structure 
 * @param[in] opts    - xpi_thread_options enumeration
 * @param[in] entry   - thread entry function
 * @param[in] args    - arguments to the thread, may be a pointer to an address, 
 *                      or a word-sized value directly. The semantics of this 
 *                      parameter are defined by the function, which users of
 *                      the function need to conform to.
 * @param[out] result - future representing result
 * @param[out] thread - address of the thread structure
 * @returns 0 for success, non-0 for error
 */
int hpx_thread_create(struct hpx_context    *ctx,
                      uint16_t              opts,
                      hpx_func_t           entry,
                      void                 *args,
                      struct hpx_future **result,
                      struct hpx_thread **thread);

void hpx_thread_destroy(struct hpx_thread *);


/**
 * @returns a pointer to the current user thread
 */
struct hpx_thread *hpx_thread_self(void);

/**
 * Get a thread's id.
 *
 * @param[in] thread NOT_NULL
 * @returns the thread's id
 */
hpx_thread_id_t hpx_thread_get_id(struct hpx_thread *thread);

/**
 * Get a thread's state.
 *
 * @param[in] thread NOT_NULL
 * @returns The thread's state (@see hpx_thread_states)
 */
hpx_thread_state_t hpx_thread_get_state(struct hpx_thread *thread);

/**
 * Get a thread's options.
 *
 * @param[in] thread NOT_NULL
 * @returns The thread's options (@see hpx_thread_options)
 */
uint16_t hpx_thread_get_opt(struct hpx_thread *thread);

/**
 * Set a thread's options.
 *
 * @todo does this overwrite the thread's options or does it | the passed opts
 *       into the current set?
 *
 * @param[in] thread NOT_NULL
 * @param[in] opts The new options (@see hpx_thread_options)
 */
void hpx_thread_set_opt(struct hpx_thread *thread, uint16_t opts);

/**
 * Terminates execution of the thread, with the passed return value.
 *
 * This transmits a word-sized value that can be retrieved by a joining thread
 * (@see hpx_thread_join). This may be the address of a (malloc-ed) buffer, or
 * a value directly. The joining thread must understand that it has to free the
 * returned buffer, if necessary.
 *
 * A thread with the HPX_THREAD_OPT_DETACHED may pass a non-null result,
 * however if it sends a dynamically allocated buffer address then it will
 * never be freed.
 *
 * @param[in] result - the result will be passed to any hpx_thread_join ers and
 *                     will set the return future
 */
void hpx_thread_exit(void *result);

/**
 * Cooperatively yields.
 *
 * This notifies the thread scheduler that this thread no longer needs to
 * execute at the current time. This call is "fair," i.e., there exists a
 * bounded amount of time that this thread may be descheduled. This call may
 * return immediately if the scheduler decides not to perform a context
 * switch.
 */
void hpx_thread_yield(void);

/**
 * @todo document this
 */
void hpx_thread_yield_skip(uint8_t);

/**
 * Wait for a future to be ready.
 *
 * This acts as a scheduling point, so even if the future is ready, the thread
 * may yield. Programmers can not assume that waiting on a "ready" future will
 * be a non-blocking operation.
 *
 * @code
 * xpi_future_t *f = {...};
 * xpi_thread_wait(f); // may yield
 * xpi_thread_wait(f); // may yield
 * @endcode
 *
 * @todo Is it safe to use this from main, which is *not* a thread but rather a
 *       pthread? If it is, then we need to be more clear about where and why
 *       this interface can be used.
 *
 * @param[in] future; the future to wait for (NOT_NULL)
 */
void hpx_thread_wait(struct hpx_future *future);

/*
 --------------------------------------------------------------------
  Private Functions
 --------------------------------------------------------------------
*/

void _hpx_thread_terminate(struct hpx_thread *);
void _hpx_thread_wait(void *, hpx_thread_wait_pred_t, void *);

/*
 --------------------------------------------------------------------
  Map Functions
 --------------------------------------------------------------------
*/

uint64_t hpx_thread_map_hash(struct hpx_map *, void *);
bool hpx_thread_map_cmp(void *, void *);


/**
 * This exposes a global thread context. This is deprecated, but some existing
 * code depends on the __hpx_global_ctx which we really don't want to expose.
 */
struct hpx_context *hpx_thread_get_global_ctx(void);


#endif /* LIBHPX_THREAD_H_ */
