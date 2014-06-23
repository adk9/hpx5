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

#include "hpx/attributes.h"
#include "libsync/lockable_ptr.h"

/// ----------------------------------------------------------------------------
/// This constant is used to determine when a set should be performed
/// asynchronously, even if the set is actually local.
/// ----------------------------------------------------------------------------
static const int HPX_LCO_SET_ASYNC = 512;

typedef struct lco_class lco_class_t;
typedef union {
  lockable_ptr_t       lock;
  const lco_class_t *vtable;
  uintptr_t            bits;
} lco_t;

/// ----------------------------------------------------------------------------
/// The LCO abstract class interface.
///
/// All LCOs will implement this interface, which is accessible through the
/// hpx_lco set of generic actions. Concrete classes may implement extended
/// interfaces as well.
///
/// This interface is locally synchronous, but will be invoked externally
/// through the set of hpx_lco_* operations that may use them asynchronously.
/// ----------------------------------------------------------------------------
typedef void (*lco_fini_t)(lco_t *lco);
typedef void (*lco_set_t)(lco_t *lco, int size, const void *value);
typedef void (*lco_error_t)(lco_t *lco, hpx_status_t code);
typedef hpx_status_t (*lco_get_t)(lco_t *lco, int size, void *value);
typedef hpx_status_t (*lco_wait_t)(lco_t *lco);


struct lco_class {
  lco_fini_t   on_fini;
  lco_error_t on_error;
  lco_set_t     on_set;
  lco_get_t     on_get;
  lco_wait_t   on_wait;
};


/// ----------------------------------------------------------------------------
/// LCO operations merely operate on the bits of an lco vtable pointer.
/// ----------------------------------------------------------------------------

/// ----------------------------------------------------------------------------
/// Lock an LCO.
///
/// @param lco - the LCO to lock
/// @returns   - the vtable pointer for the LCO, with any packed state removed
/// ----------------------------------------------------------------------------
HPX_INTERNAL const lco_class_t *lco_lock(lco_t *lco) HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// Unlock an LCO.
///
/// The calling thread must currently hold the LCO's lock.
///
/// @param lco - the LCO to unlock
/// ----------------------------------------------------------------------------
HPX_INTERNAL void lco_unlock(lco_t* lco) HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// Initialize an LCO vtable pointer.
///
/// @param   lco - the pointer to initialize
/// @param class - the class pointer for this LCO instance
/// @param  user - non-zero to set the user state during initialization
/// ----------------------------------------------------------------------------
HPX_INTERNAL void lco_init(lco_t *lco, const lco_class_t *class, uintptr_t user)
  HPX_NON_NULL(1,2);


/// ----------------------------------------------------------------------------
/// Set the user state bit to one.
///
/// This operation does not acquire the LCO lock---the caller must lock the
/// pointer first if this could occur concurrently.
///
/// @param lco - the LCO to reset
/// ----------------------------------------------------------------------------
HPX_INTERNAL void lco_set_user(lco_t *lco);


/// ----------------------------------------------------------------------------
/// Resets the user state bit to zero.
///
/// This operation does not acquire the LCO lock---the caller must lock the
/// pointer first if this could occur concurrently.
///
/// @param lco - the LCO to reset
/// ----------------------------------------------------------------------------
HPX_INTERNAL void lco_reset_user(lco_t *lco);


/// ----------------------------------------------------------------------------
/// Get the user state bit.
///
/// This operation does not acquire the LCO lock---the caller must lock the
/// pointer first if this could occur concurrently.
///
/// @param lco - the LCO to read
/// @returns   - non-zero if the user state bit is set, zero otherwise
/// ----------------------------------------------------------------------------
HPX_INTERNAL uintptr_t lco_get_user(const lco_t *lco);


/// ----------------------------------------------------------------------------
/// Set the triggered state to true.
///
/// This operation does not acquire the LCO lock---the caller must lock the
/// pointer first if this could occur concurrently.
///
/// @param lco - the LCO to trigger
/// ----------------------------------------------------------------------------
HPX_INTERNAL void lco_set_triggered(lco_t *lco);


/// ----------------------------------------------------------------------------
/// Reset the triggered state to false.
///
/// This operation does not acquire the LCO lock---the caller must lock the
/// pointer first if this could occur concurrently.
///
/// @param lco - the LCO to trigger
/// ----------------------------------------------------------------------------
HPX_INTERNAL void lco_reset_triggered(lco_t *lco);


/// ----------------------------------------------------------------------------
/// Get the triggered state.
///
/// This operation does not acquire the LCO lock---the caller must lock the
/// pointer first if this could occur concurrently.
///
/// @param lco - the LCO to read
/// @returns   - non-zero if the triggered bit is set, zero otherwise
/// ----------------------------------------------------------------------------
HPX_INTERNAL uintptr_t lco_get_triggered(const lco_t *lco);


#endif // LIBHPX_LCO_H
