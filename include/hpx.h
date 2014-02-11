/*
 ====================================================================
  High Performance ParalleX Library (libhpx)

  Thread Function Definitions
  hpx_thread.h

  Copyright (c) 2013, Trustees of Indiana University
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/

#ifndef HPX_H_
#define HPX_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "attributes.h"

typedef uintptr_t hpx_addr_t;
#define HPX_NULL ((uintptr_t)NULL)

/**
 * The type of the key we use for actions.
 */
typedef uintptr_t hpx_action_t;

/**
 * This is the C-type for action handlers.
 */
typedef int (*hpx_action_handler_t)(void *);

/**
 * printf format arguments for the action
 */
#define HPX_PRIx_hpx_action_t PRIxPTR
#define HPX_PRId_hpx_action_t PRIdPTR
#define HPX_PRIX_hpx_action_t PRIXPTR
#define HPX_PRIu_hpx_action_t PRIuPTR
#define HPX_PRIo_hpx_action_t PRIoPTR

/**
 * The NULL action.
 */
#define HPX_ACTION_NULL ((uintptr_t)NULL)

/**
 * Register an action with the runtime.
 *
 * @param[in]   id - a unique string name for the action
 * @param[in] func - the local function pointer to associate with the action
 * @returns        - a key to be used for the action when needed
 */
hpx_action_t hpx_action_register(const char *id, hpx_action_handler_t func);

/**
 * Call after all actions are registered.
 */
void hpx_action_registration_complete(void);

hpx_action_t hpx_action_register();

int hpx_init(int argc, char * const argv[argc]);
int hpx_run(hpx_action_t f, void *args, unsigned size);
void hpx_shutdown(int) HPX_NORETURN;

int hpx_get_my_rank(void);
int hpx_get_num_ranks(void);

void hpx_thread_wait(hpx_addr_t future, void *value);
void hpx_thread_wait_all(unsigned n, hpx_addr_t futures[], void *values[]);
void hpx_thread_yield(void);
int hpx_thread_exit(void *value, unsigned size);

hpx_addr_t hpx_future_new(int size);
void hpx_future_delete(hpx_addr_t future);

/**
 * Perform an @p action at a @p location and get a result through a future.
 *
 * @todo
 *
 * @param[in] location - the location at which to perform the action
 */
void hpx_call(int rank, hpx_action_t action, void *args, size_t len, hpx_addr_t result);


typedef uint64_t hpx_time_t;
hpx_time_t hpx_time_now(void);
uint64_t hpx_time_to_us(hpx_time_t);
uint64_t hpx_time_to_ms(hpx_time_t);

#ifdef __cplusplus
}
#endif

#endif  // HPX_H
