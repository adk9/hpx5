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

#include <stdbool.h>
#include "hpx/attributes.h"

typedef struct lco_node lco_node_t;
typedef struct lco lco_t;


/// ----------------------------------------------------------------------------
/// The LCO abstract class interface.
///
/// The concrete class implementation is responsible for ensuring that these are
/// serializable, and that set() and get() properly wait/signal the LCO, as
/// needed.
/// ----------------------------------------------------------------------------
typedef struct {
  void (*delete)(lco_t *lco);
  void (*set)(lco_t *lco, int size, const void *value, hpx_status_t status);
  hpx_status_t (*get)(lco_t *lco, int size, void *value);
} lco_class_t;


#define LCO_CLASS_INIT(d, s, g) {                                   \
    .delete = (void (*)(lco_t*))d,                                  \
    .set = (void (*)(lco_t*, int, const void *, hpx_status_t))s,    \
    .get = (hpx_status_t (*)(lco_t*, int, void *))g                 \
    }


/// ----------------------------------------------------------------------------
/// LCOs contain queues of waiting stuff.
/// ----------------------------------------------------------------------------
struct lco_node {
  lco_node_t *next;
  void       *data;
};


/// ----------------------------------------------------------------------------
/// The base class for LCOs.
/// ----------------------------------------------------------------------------
struct lco {
  const lco_class_t *vtable;
  lco_node_t *queue;
};


/// ----------------------------------------------------------------------------
/// Initializes an LCO.
///
/// If @p user is non-null, then the initial value of the USER state bit is set,
/// otherwise it is unset.
/// ----------------------------------------------------------------------------
HPX_INTERNAL void lco_init(lco_t *lco, const lco_class_t *class, int user)
  HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// Finalizes an lco.
/// ----------------------------------------------------------------------------
HPX_INTERNAL void lco_fini(lco_t *lco) HPX_NON_NULL(1);


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


HPX_INTERNAL hpx_status_t lco_get_status(const lco_t *lco)
  HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// Resets the set state for the LCO.
///
/// This is not synchronized. The caller should already hold the lock on the LCO
/// if the call might be concurrent with any other operations on the LCO. The
/// set state is set during lco_trigger().
///
/// @param lco - the LCO to reset
/// ----------------------------------------------------------------------------
HPX_INTERNAL void lco_reset(lco_t *lco) HPX_NON_NULL(1);


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
/// Atomically sets the LCO and returns the wait queue.
///
/// This does not try to acquire or release the lco's lock.
///
/// @precondition The calling thread must hold the lco's lock already.
/// ----------------------------------------------------------------------------
HPX_INTERNAL lco_node_t *lco_trigger(lco_t *lco, hpx_status_t status)
  HPX_NON_NULL(1);


HPX_INTERNAL void lco_enqueue(lco_t *lco, lco_node_t *node)
  HPX_NON_NULL(1, 2);


/// ----------------------------------------------------------------------------
/// Atomically add a thread to the LCO's wait queue, and unlock the LCO.
///
/// @precondition The calling thread must hold the lco's lock already.
/// ----------------------------------------------------------------------------
HPX_INTERNAL void lco_enqueue_and_unlock(lco_t *lco, lco_node_t *node)
  HPX_NON_NULL(1, 2);


#endif // LIBHPX_LCO_H
