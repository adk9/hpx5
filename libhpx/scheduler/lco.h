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
#ifndef LIBHPX_LCO_H
#define LIBHPX_LCO_H

#include <hpx/attributes.h>
#include <jemalloc/jemalloc_hpx.h>
#include <libsync/lockable_ptr.h>

/// This constant is used to determine when a set should be performed
/// asynchronously, even if the set is actually local.
static const int HPX_LCO_SET_ASYNC = 512;

typedef struct lco_class lco_class_t;
typedef union {
  lockable_ptr_t       lock;
  const lco_class_t *vtable;
  uintptr_t            bits;
} lco_t HPX_ALIGNED(16);


/// The LCO abstract class interface.
///
/// All LCOs will implement this interface, which is accessible through the
/// hpx_lco set of generic actions. Concrete classes may implement extended
/// interfaces as well.
///
/// This interface is locally synchronous, but will be invoked externally
/// through the set of hpx_lco_* operations that may use them asynchronously.
typedef void (*lco_fini_t)(lco_t *lco);
typedef void (*lco_set_t)(lco_t *lco, int size, const void *value);
typedef void (*lco_error_t)(lco_t *lco, hpx_status_t code);
typedef hpx_status_t (*lco_get_t)(lco_t *lco, int size, void *value);
typedef hpx_status_t (*lco_wait_t)(lco_t *lco);
typedef hpx_status_t (*lco_attach_t)(lco_t *lco, hpx_parcel_t *p);
typedef hpx_status_t (*lco_try_get_t)(lco_t *lco, int size, void *value, hpx_time_t time);
typedef hpx_status_t (*lco_try_wait_t)(lco_t *lco, hpx_time_t time);


struct lco_class {
  lco_fini_t         on_fini;
  lco_error_t       on_error;
  lco_set_t           on_set;
  lco_attach_t     on_attach;
  lco_get_t           on_get;
  lco_wait_t         on_wait;
  lco_try_get_t   on_try_get;
  lco_try_wait_t on_try_wait;
} HPX_ALIGNED(16);

// -----------------------------------------------------------------------------
// LCO operations merely operate on the bits of an lco vtable pointer.
// -----------------------------------------------------------------------------

/// Lock an LCO.
///
/// @param lco  The LCO to lock
void lco_lock(lco_t *lco)
  HPX_INTERNAL HPX_NON_NULL(1);

/// Unlock an LCO.
///
/// The calling thread must currently hold the LCO's lock.
///
/// @param lco - the LCO to unlock
void lco_unlock(lco_t* lco)
  HPX_INTERNAL HPX_NON_NULL(1);

/// Initialize an LCO vtable pointer.
///
/// @param           lco The pointer to initialize
/// @param         class The class pointer for this LCO instance
void lco_init(lco_t *lco, const lco_class_t *class)
  HPX_INTERNAL HPX_NON_NULL(1,2);

/// Finalize an LCO vtable pointer.
///
/// @param           lco The pointer to finalize.
void lco_fini(lco_t *lco)
  HPX_INTERNAL HPX_NON_NULL(1);

/// Resets the user state bit to zero.
///
/// This operation does not acquire the LCO lock---the caller must lock the
/// pointer first if this could occur concurrently.
///
/// @param           lco The LCO to reset.
void lco_reset_user(lco_t *lco)
  HPX_INTERNAL HPX_NON_NULL(1);

/// Get the user state bit.
///
/// This operation does not acquire the LCO lock---the caller must lock the
/// pointer first if this could occur concurrently.
///
/// @param           lco The LCO to read.
/// @returns Non-zero if the user state bit is set, zero otherwise
uintptr_t lco_get_user(const lco_t *lco)
  HPX_INTERNAL HPX_NON_NULL(1);

/// Set the triggered state to true.
///
/// This operation does not acquire the LCO lock---the caller must lock the
/// pointer first if this could occur concurrently.
///
/// @param           lco The LCO to trigger.
void lco_set_triggered(lco_t *lco)
  HPX_INTERNAL HPX_NON_NULL(1);

/// Reset the triggered state to false.
///
/// This operation does not acquire the LCO lock---the caller must lock the
/// pointer first if this could occur concurrently.
///
/// @param           lco The LCO to trigger.
void lco_reset_triggered(lco_t *lco)
  HPX_INTERNAL HPX_NON_NULL(1);

/// Get the triggered state.
///
/// This operation does not acquire the LCO lock---the caller must lock the
/// pointer first if this could occur concurrently.
///
/// @param           lco The LCO to read.
///
/// @returns Non-zero if the triggered bit is set, zero otherwise.
uintptr_t lco_get_triggered(const lco_t *lco)
  HPX_INTERNAL HPX_NON_NULL(1);

#endif // LIBHPX_LCO_H
