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

#include "attributes.h"

struct thread;

typedef struct lco {
  struct thread *queue;
} lco_t;

/// ----------------------------------------------------------------------------
/// Initializes an LCO.
///
/// If @p user is non-null, then the initial value of the USER state bit is set,
/// otherwise it is unset.
/// ----------------------------------------------------------------------------
HPX_INTERNAL void lco_init(lco_t *lco, int user) HPX_NON_NULL(1);

/// ----------------------------------------------------------------------------
/// Set the USER state bit on the LCO.
///
/// This is not synchronized. The caller should hold the lock on the LCO, if the
/// call might be concurrent with any other operation on the LCO.
/// ----------------------------------------------------------------------------
HPX_INTERNAL void lco_set_user(lco_t *lco) HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// Get the USER state bit on the LCO.
///
/// This is not synchronized. The caller should already hold the lock on the LCO
/// if the call might be concurrent with any other operations on the LCO. This
/// returns 0 if the bit is not set, and non-zero if the bit is set. It does not
/// return 1.
/// ----------------------------------------------------------------------------
HPX_INTERNAL int lco_is_user(const lco_t *lco) HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// Gets the SET state bit on the LCO.
///
/// This is not synchronized. The caller should already hold the lock on the LCO
/// if the call might be concurrent with any other operations on the LCO. This
/// returns 0 if the bit is not set, and non-zero if the bit is set. It does not
/// return 1.
/// ----------------------------------------------------------------------------
HPX_INTERNAL int lco_is_set(const lco_t *lco) HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// Acquire the LCO's lock.
///
/// This uses a scheduler-aware algorithm, and will scheduler_yield() if the LCO
/// is currently locked. LCO locks are not reentrant.
///
/// @precondition The current thread does not already hold the lock on the LCO.
/// ----------------------------------------------------------------------------
HPX_INTERNAL void lco_lock(lco_t *lco) HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// Unlock the LCO.
///
/// @precondition The current thread already holds the lock on the LCO.
/// ----------------------------------------------------------------------------
HPX_INTERNAL void lco_unlock(lco_t *lco) HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// Atomically sets the LCO as triggered and returns the wait queue.
///
/// This does not try to acquire or release the lco's lock.
///
/// @precondition The calling thread must hold the lco's lock already.
/// ----------------------------------------------------------------------------
HPX_INTERNAL struct thread *lco_trigger(lco_t *lco)
  HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// Atomically add a thread to the LCO's wait queue, and unlock the LCO.
///
/// @precondition The calling thread must hold the lco's lock already.
/// ----------------------------------------------------------------------------
HPX_INTERNAL void lco_enqueue_and_unlock(lco_t *lco, struct thread *thread)
  HPX_NON_NULL(1, 2);

#endif // LIBHPX_LCO_H
