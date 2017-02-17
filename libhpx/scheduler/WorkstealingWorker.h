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

#ifndef LIBHPX_SCHEDULER_WORKSTEALING_WORKER_H
#define LIBHPX_SCHEDULER_WORKSTEALING_WORKER_H

#include "libhpx/Worker.h"
#include "libhpx/util/Aligned.h"
#include "libhpx/util/ChaseLevDeque.h"
#include "libhpx/util/TwoLockQueue.h"
#include <random>

namespace libhpx {
namespace scheduler {
class WorkstealingWorker : public WorkerBase,
                           public libhpx::util::Aligned<HPX_CACHELINE_SIZE>
{
  static constexpr int MAGIC_STEAL_HALF_THRESHOLD = 6;

  using Deque = libhpx::util::ChaseLevDeque<hpx_parcel_t*>;

 public:
  WorkstealingWorker(Scheduler& sched, int id);
  ~WorkstealingWorker();

  /// Asynchronous entry point for the steal-half operation.
  ///
  /// @todo This is public because we register it with the LIBHPX_ACTION
  ///       macro. We should increase its protection if possible.
  ///
  /// @param        src The source of the steal request.
  static int StealHalfHandler(WorkstealingWorker* src);

 protected:
  void onSpawn(hpx_parcel_t*);

  hpx_parcel_t* onSchedule() {
    return popLIFO();
  }

  hpx_parcel_t* onBalance() {
    return handleSteal();
  }

  void onSleep() {
  }

 private:
  hpx_parcel_t* handleSteal();

  /// Pop the next available parcel from our lifo work queue.
  hpx_parcel_t* popLIFO();

  /// Push a parcel into the lifo queue.
  void pushLIFO(hpx_parcel_t *p);

  /// All of the steal functionality.
  ///
  /// @todo We should extract stealing policies into a policy class that is
  ///       initialized based on the runtime configuration.
  /// @{
  hpx_parcel_t* stealFrom(WorkstealingWorker* victim);
  hpx_parcel_t* stealHalf();
  hpx_parcel_t* stealRandom();
  hpx_parcel_t* stealRandomNode();
  hpx_parcel_t* stealHierarchical();
  /// @}

  static WorkstealingWorker* GetWorker(int id);

  const int                           numaNode_; //!< this worker's numa node
  std::mt19937&                            rng_; //!< reference to my mt19937
  std::uniform_int_distribution<int>       cpu_; //!< random cpu
  std::uniform_int_distribution<int>      node_; //!< random node
  std::uniform_int_distribution<int> cpuOnNode_; //!< random node local cpu
  int                                workFirst_; //!< this worker's mode
  WorkstealingWorker*               lastVictim_; //!< last successful victim
  Deque                                   work_; //!< work queue
}; // class WorkstealingWorker
} // namespace scheduler
} // namespace libhpx

#endif // #define LIBHPX_SCHEDULER_WORKSTEALING_Worker_H
