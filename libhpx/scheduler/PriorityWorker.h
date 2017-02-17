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

#ifndef LIBHPX_SCHEDULER_PRIORITY_WORKER_H
#define LIBHPX_SCHEDULER_PRIORITY_WORKER_H

#include "libhpx/Worker.h"
#include "libhpx/util/Aligned.h"

namespace libhpx {
namespace scheduler {
class PriorityWorker : public WorkerBase,
                       public util::Aligned<HPX_CACHELINE_SIZE>
{
 public:
  PriorityWorker(Scheduler& sched, int id)
      : WorkerBase(sched, id),
        util::Aligned<HPX_CACHELINE_SIZE>()
  {}
};
} // namespace scheduler
} // namespace libhpx

#endif // #define LIBHPX_SCHEDULER_PRIORITY_WORKER_H
