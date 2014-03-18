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
#ifndef LIBHPX_WORKER_H
#define LIBHPX_WORKER_H

#include "attributes.h"

struct scheduler;
typedef struct worker worker_t;

/// ----------------------------------------------------------------------------
/// Starts a worker thread associated with a scheduler.
///
/// @param      id - the worker's id
/// @param core_id - the core id this worker's native thread should run on
/// @param   sched - the sched instance this worker is associated with
/// @returns       - a worker object
/// ----------------------------------------------------------------------------
HPX_INTERNAL worker_t *worker_start(int id, int core_id,
                                    struct scheduler *sched)
  HPX_NON_NULL(3);


/// ----------------------------------------------------------------------------
/// Cooperatively shutdown a worker.
///
/// This will block until the worker's native thread exits.
///
/// @param worker - the worker to shutdown
/// ----------------------------------------------------------------------------
HPX_INTERNAL void worker_shutdown(worker_t *worker)
  HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// Preemptively shutdown a worker.
///
/// This will block until the worker's native thread exits. This will leave the
/// (UNIX) process in an undefined state. Only async-safe functions can be used
/// after this routine.
///
/// @param worker - the worker to cancel
/// ----------------------------------------------------------------------------
HPX_INTERNAL void worker_cancel(worker_t *worker)
  HPX_NON_NULL(1);


#endif // LIBHPX_WORKER_H
