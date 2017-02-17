// =============================================================================
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

extern "C" {
#include "linden/prioq.h"
#include "linden/gc/gc.h"
}

#include "libhpx/util/PriorityQueue.h"
#include "libhpx/util/TwoLockQueue.h"

using namespace libhpx::util;

namespace {
class BrokenQueue : public PriorityQueue, public TwoLockQueue<hpx_parcel_t*> {
  using Base = TwoLockQueue<hpx_parcel_t*>;
 public:
  BrokenQueue() : PriorityQueue(), Base() {
  }

  void insert(int key, hpx_parcel_t* value) {
    Base::enqueue(value);
  }

  hpx_parcel_t* deleteMin() {
    return Base::dequeue();
  }
};

class LindenQueue : public PriorityQueue {
 public:
  LindenQueue(int offset) : pq_(pq_init(offset)) {
  }

  ~LindenQueue() {
    pq_destroy(pq_);
  }

  void insert(int key, hpx_parcel_t* value) {
    ::insert(pq_, key, value);
  }

  hpx_parcel_t* deleteMin() {
    return static_cast<hpx_parcel_t*>(deletemin(pq_));
  }

 private:
  pq_t *pq_;
};

class GC {
 public:
  GC() : init_(false) {
  }

  ~GC() {
    if (init_) _destroy_gc_subsystem();
  }

  void init() {
    if (!init_) _init_gc_subsystem();
  }

 private:
  bool init_;
};

GC gc{};
}

PriorityQueue::~PriorityQueue()
{
}

PriorityQueue*
PriorityQueue::Create(const config_t* cfg)
{
  if (cfg->sched == HPX_SCHED_LINDEN) {
    gc.init();
    return new LindenQueue(64);
  }
  else {
    return new BrokenQueue();
  }
}
