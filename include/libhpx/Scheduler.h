// ==================================================================-*- C++ -*-
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2016, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================

#ifndef LIBHPX_C_SCHEDULER_H
#define LIBHPX_C_SCHEDULER_H

#include "libhpx/padding.h"
#include "libhpx/Worker.h"

//#include <libsync/queues.h>

/// Preprocessor define that tells us if the scheduler is cooperative or
/// preemptive. Unused at this point.
/// @{
#define LIBHPX_SCHEDULER_COOPERATIVE 1
//#define LIBHPX_SCHEDULER_PREEMPTIVE 1
/// @}

/// Scheduler states.
/// @{
enum {
  SCHED_SHUTDOWN,
  SCHED_STOP,
  SCHED_RUN,
};
/// @}

/// The scheduler class.
///
/// The scheduler class represents the shared-memory state of the entire
/// scheduling process. It serves as a collection of native worker threads, and
/// a network port, and allows them to communicate with each other and the
/// network.
///
/// It is possible to have multiple scheduler instances active within the same
/// memory space---though it is unclear why we would need or want that at this
/// time---and it is theoretically possible to move workers between schedulers
/// by updating the worker's scheduler pointer and the scheduler's worker
/// table, though all of the functionality that is required to make this work is
/// not implemented.
struct Scheduler {
  pthread_mutex_t     lock;                  //!< lock for running condition
  pthread_cond_t   stopped;                  //!< the running condition
  volatile int       state;                  //!< the run state
  volatile int next_tls_id;                  //!< lightweight thread ids
  volatile int        code;                  //!< the exit code
  volatile int    n_active;                  //!< active number of workers
  int            n_workers;                  //!< total number of workers
  int             n_target;                  //!< target number of workers
  int                epoch;                  //!< current scheduler epoch
  int                 spmd;                  //!< 1 if the current epoch is spmd
  long             ns_wait;                  //!< nanoseconds to wait in start()
  void             *output;                  //!< the output slot
  PAD_TO_CACHELINE(sizeof(pthread_mutex_t) +
                   sizeof(pthread_cond_t) +
                   sizeof(int) * 10 +
                   sizeof(long) +
                   sizeof(void*));            //!< padding to align workers
  libhpx::Worker*  workers[];                 //!< array of worker data
};

#endif // LIBHPX_C_SCHEDULER_H
