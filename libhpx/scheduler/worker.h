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

#include "hpx/attributes.h"

struct scheduler;
typedef struct worker worker_t;


/// ----------------------------------------------------------------------------
/// The main entry function for a scheduler worker thread.
///
/// Each worker will self-assign an ID. IDs are guaranteed to be dense,
/// contiguous integers.
/// ----------------------------------------------------------------------------
HPX_INTERNAL void *worker_run(struct scheduler *sched)
  HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// Starts a worker thread associated with a scheduler.
///
/// @param      id - the worker's id
/// @param   sched - the sched instance this worker is associated with
/// @returns       - HPX_SUCCESS or HPX_ERROR
/// ----------------------------------------------------------------------------
HPX_INTERNAL int worker_start(struct scheduler *s)
  HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// Cooperatively shutdown a worker.
///
/// This does not block.
///
/// @param worker - the worker to shutdown
/// ----------------------------------------------------------------------------
HPX_INTERNAL void worker_shutdown(worker_t *worker)
  HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// Joins a worker after worker_shutdown().
///
/// This is done separately to allow for cleanup to happen. Also improves
/// shutdown performance because all of the workers can be shutting down and
/// cleaning up in parallel. Workers don't generate values at this point.
///
/// @param worker - the worker to join
/// ----------------------------------------------------------------------------
HPX_INTERNAL void worker_join(worker_t *worker)
  HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// Preemptively shutdown a worker.
///
/// This will leave the (UNIX) process in an undefined state. Only async-safe
/// functions can be used after this routine. This is non-blocking, canceled
/// threads should be joined if you need to wait for them.
///
/// @param worker - the worker to cancel
/// ----------------------------------------------------------------------------
HPX_INTERNAL void worker_cancel(worker_t *worker)
  HPX_NON_NULL(1);


#endif // LIBHPX_WORKER_H
