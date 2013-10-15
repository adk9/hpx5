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

#pragma once
#ifndef LIBHPX_THREAD_H_
#define LIBHPX_THREAD_H_

#include <stdarg.h>

#include "hpx/mem.h"
#include "hpx/lco.h"
#include "hpx/types.h"
#include "hpx/thread/mctx.h"
#include "hpx/utils/list.h"
#include "hpx/utils/map.h"

typedef uint64 hpx_node_id_t;
typedef uint64 hpx_thread_id_t;
typedef uint8  hpx_thread_state_t;

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
 * @param[in][out]
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
struct hpx_thread_reusable_t {
  hpx_func_t           func;                    /**< entry function  */
  void                *args;                    /**< arguments */
  void                *stk;                     /**< stack  */
  size_t               ss;                      /**< stack size  */
  hpx_mctx_context_t  *mctx;                   /**< machine context (jmpbuf) */
  void                *wait;                    /**< waiting future  */
  hpx_kthread_t       *kth;                     /**< kernel thread */
};

/**
 * The user-level thread data.
 */
struct hpx_thread {
  hpx_context_t          *ctx;                  /**< pointer to the context */
  hpx_node_id_t           nid;                  /**< node id */
  hpx_thread_id_t         tid;                  /**< thread id */
  hpx_thread_state_t      state; /**< queuing state (@see hpx_thread_states) */
  uint16                  opts;                 /**< thread option flags  */
  uint8                   skip;                 /**< [reserved for future] */
  hpx_thread_reusable_t  *reuse;             /**< @see hpx_thread_reusable_t */
  hpx_future_t           *f_ret;                /**< return value (if any) */
  hpx_thread_t           *parent;               /**< parent thread  */
  hpx_list_t              children;             /**< list of children */
};

/**
 * Creates a user-level thread in the local virtual environment.
 *
 * @todo Was the returned future malloced? Does it need to be freed? What if
 *       the caller doesn't care? Should it be a parameter that we can test for
 *       NULL inside the create call to suppress creation where we don't care?
 *
 * @param[in] ctx
 * @param[in] opts (@see enum xpi_thread_options)
 * @param[in] entry Thread entry function
 * @param[in] args Arguments to the thread, may be a pointer to an address, or
 *                 a word-sized value directly. The semantics of this parameter
 *                 are defined by the function, which users of the function
 *                 need to conform to. 
 * @param[out] thread Address of the thread structure
 * @returns a future that represents the returned value of the thread
 */
hpx_future_t *hpx_thread_create(hpx_context_t *ctx,
                                uint16         opts,
                                hpx_func_t     entry,
                                void          *args,
                                hpx_thread_t **thread);

void hpx_thread_destroy(hpx_thread_t *);


/**
 * @returns a pointer to the current user thread
 */
hpx_thread_t *hpx_thread_self(void);

/**
 * Get a thread's id.
 *
 * @param[in] thread NOT_NULL
 * @returns the thread's id
 */
hpx_thread_id_t hpx_thread_get_id(hpx_thread_t *thread);

/**
 * Get a thread's state.
 *
 * @param[in] thread NOT_NULL
 * @returns The thread's state (@see hpx_thread_states)
 */
hpx_thread_state_t hpx_thread_get_state(hpx_thread_t *thread);

/**
 * Get a thread's options.
 *
 * @param[in] thread NOT_NULL
 * @returns The thread's options (@see hpx_thread_options)
 */
uint16 hpx_thread_get_opt(hpx_thread_t *thread);

/**
 * Set a thread's options.
 *
 * @todo does this overwrite the thread's options or does it | the passed opts
 *       into the current set?
 *
 * @param[in] thread NOT_NULL
 * @param[in] opts The new options (@see hpx_thread_options)
 */
void hpx_thread_set_opt(hpx_thread_t *thread, uint16 opts);

/**
 * Wait for the thread to terminate.
 *
 * The thread must not have been created using the HPX_THREAD_OPT_DETACHED flag
 * (@see hpx_thread_options). This serves as a scheduler point and may result
 * in a yield, even if the thread that's being joined has already terminated.
 *
 * @todo is this really what HPX_THREAD_OPT_DETACHED means?
 *
 * @param[in] thread
 * @param[out] result The address of a location that will be set to the value
 *                    passed by thread to exit (@see hpx_thread_exit).
 */
void hpx_thread_join(hpx_thread_t *thread, void **result);

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
 * @param[in] result
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
void hpx_thread_yield_skip(uint8);

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
void hpx_thread_wait(hpx_future_t *future);

/*
 --------------------------------------------------------------------
  Private Functions
 --------------------------------------------------------------------
*/

void _hpx_thread_terminate(hpx_thread_t *);
void _hpx_thread_wait(void *, hpx_thread_wait_pred_t, void *);

/*
 --------------------------------------------------------------------
  Map Functions
 --------------------------------------------------------------------
*/

uint64_t hpx_thread_map_hash(hpx_map_t *, void *);
bool hpx_thread_map_cmp(void *, void *);

#endif /* LIBHPX_THREAD_H_ */
