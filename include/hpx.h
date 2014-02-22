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

#include "attributes.h"

/// External HPX typedefs
/// @{
typedef         uintptr_t hpx_addr_t;
typedef          uint64_t hpx_time_t;
typedef         uintptr_t hpx_action_t;
typedef            int (* hpx_action_handler_t)(void *);
typedef struct hpx_parcel hpx_parcel_t;
/// @}

extern hpx_action_t HPX_ACTION_NULL;

/// Extern HPX macros
/// @{
#define           HPX_SUCCESS 0
#define              HPX_NULL ((uintptr_t)NULL)
/// @}

/// printf formats
/// @{
#define HPX_PRIx_hpx_action_t PRIxPTR
#define HPX_PRId_hpx_action_t PRIdPTR
#define HPX_PRIX_hpx_action_t PRIXPTR
#define HPX_PRIu_hpx_action_t PRIuPTR
#define HPX_PRIo_hpx_action_t PRIoPTR
/// @}

/**
 * Register an action with the runtime.
 *
 * @param[in]   id - a unique string name for the action
 * @param[in] func - the local function pointer to associate with the action
 * @returns        - a key to be used for the action when needed
 */
hpx_action_t hpx_action_register(const char *id, hpx_action_handler_t func);

/// HPX runtime interface
int hpx_init(int argc, char * const argv[]);
int hpx_run(hpx_action_t entry, const void *args, unsigned size);
void hpx_shutdown(int code) HPX_NORETURN;

/// HPX locality interface
int hpx_get_my_rank(void);
int hpx_get_my_thread_id(void);
int hpx_get_num_ranks(void);

/// HPX address interface
int hpx_addr_to_rank(hpx_addr_t addr);
hpx_addr_t hpx_addr_from_rank(int rank);

/// HPX user-level threading interface
void hpx_thread_exit(int status, const void *value, unsigned size) HPX_NORETURN;

/// Futures are a kind of LCO and are native to HPX.
hpx_addr_t hpx_future_new(int size);
void hpx_future_delete(hpx_addr_t future);
void hpx_future_get(hpx_addr_t future, void *value, int size);
void hpx_future_get_all(unsigned n, hpx_addr_t futures[], void *values[], const int sizes[]);
void hpx_future_set(hpx_addr_t future, const void *value, int size);

/// HPX high-resolution timer interface
hpx_time_t hpx_time_now(void);
uint64_t hpx_time_to_us(hpx_time_t);
uint64_t hpx_time_to_ms(hpx_time_t);

/// HoPX parcel interface
hpx_parcel_t *hpx_parcel_acquire(unsigned);
void hpx_parcel_set_action(hpx_parcel_t *p, hpx_action_t action);
void hpx_parcel_set_target(hpx_parcel_t *p, hpx_addr_t addr);
void hpx_parcel_set_cont(hpx_parcel_t *p, hpx_addr_t lco);
void hpx_parcel_set_data(hpx_parcel_t *p, const void *data, int size);
void *hpx_parcel_get_data(hpx_parcel_t *p);
void hpx_parcel_send(hpx_parcel_t *p);
void hpx_parcel_send_sync(hpx_parcel_t *p);

/// HPX rpc interface
void hpx_call(hpx_addr_t addr, hpx_action_t action, const void *args,
              size_t len, hpx_addr_t result);

#ifdef __cplusplus
}
#endif

#endif // HPX_H
