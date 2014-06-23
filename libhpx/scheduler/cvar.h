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
#ifndef LIBHPX_SCHEDULER_CVAR_H
#define LIBHPX_SCHEDULER_CVAR_H

#include "libhpx/scheduler.h"

typedef struct cvar {
  struct ustack *top;
} cvar_t;


/// ----------------------------------------------------------------------------
/// Reset a condition variable.
/// ----------------------------------------------------------------------------
HPX_INTERNAL void cvar_reset(cvar_t *cvar)
  HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// Set the condition with an error code.
///
/// This records the error in the condition, and returns the list of stacks that
/// were waiting. The error code will overwrite this list, so it must be
/// processed here.
///
/// @param cvar - the condition variable to set an error in
/// @param code - the user-defined error code
/// @returns    - the previous list of waiting threads
/// ----------------------------------------------------------------------------
HPX_INTERNAL struct ustack *cvar_set_error(cvar_t *cvar, hpx_status_t code)
  HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// Retrieve an error code from the condition variable.
///
/// This will check to see if an error is set on a condition variable, and
/// return it if there is one.
///
/// @param cvar - the condition variable to check
/// @returns    - HPX_SUCCESS or an error code
/// ----------------------------------------------------------------------------
HPX_INTERNAL hpx_status_t cvar_get_error(const cvar_t *cvar)
  HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// Clear an error condition.
///
/// If there is no error condition set, then this does not modify the condition
/// variable, otherwise it clears the error.
///
/// @param cvar - the condition variable to update
/// ----------------------------------------------------------------------------
HPX_INTERNAL void cvar_clear_error(cvar_t *cvar)
  HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// Push a thread onto a condition variable.
///
/// If the condition is in an error condition, this will return that error
/// without pushing the thread, otherwise it will push the thread and return
/// HPX_SUCCESS.
///
/// @param   cvar - the condition variable to modify
/// @param thread - the thread to push
/// @returns      - HPX_SUCCESS or an error code
/// ----------------------------------------------------------------------------
HPX_INTERNAL hpx_status_t cvar_push_thread(cvar_t *cvar, struct ustack *thread)
  HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// Pop the top thread from a condition variable.
///
/// @param cvar - the condition to pop
/// @return     - the top thread, or NULL if the condition is empty or has an
///               error
/// ----------------------------------------------------------------------------
HPX_INTERNAL struct ustack *cvar_pop_thread(cvar_t *cvar)
  HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// Pop the top thread from a condition variable.
///
/// @param cvar - the condition to pop
/// @return     - the thread's list
/// ----------------------------------------------------------------------------
HPX_INTERNAL struct ustack *cvar_pop_all(cvar_t *cvar)
  HPX_NON_NULL(1);

#endif // LIBHPX_SCHEDULER_CVAR_H
