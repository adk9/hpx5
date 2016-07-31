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

#ifndef LIBSYNC_QUEUES_HPP
#define LIBSYNC_QUEUES_HPP

#include "libsync/queues.h"

namespace libsync {
template <typename T>
class TwoLockQueue {
};

template <typename T>
class TwoLockQueue<T*> : public two_lock_queue_t {
 public:
  TwoLockQueue() {
    sync_two_lock_queue_init(this, NULL);
  }

  ~TwoLockQueue() {
    sync_two_lock_queue_fini(this);
  }

  T* dequeue() {
    return static_cast<T*>(sync_two_lock_queue_dequeue(this));
  }

  void enqueue(T* t) {
    sync_two_lock_queue_enqueue(this, t);
  }
};
}

#endif // LIBSYNC_QUEUES_HPP
