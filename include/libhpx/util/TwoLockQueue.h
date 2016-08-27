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

#ifndef LIBHPX_UTIL_TWO_LOCK_QUEUE_H
#define LIBHPX_UTIL_TWO_LOCK_QUEUE_H

#include "libhpx/util/Aligned.h"             // template Align
#include "libsync/queues.h"                  // two_lock_queue
#include "hpx/hpx.h"                         // HPX_CACHELINE_SIZE, hpx_parcel_t
#include <cassert>                           // assert
#include <cstdint>                           // uintptr_t

namespace libhpx {
namespace util {

template <typename T>
class TwoLockQueue : public Aligned<HPX_CACHELINE_SIZE>
{
 public:
  TwoLockQueue() : queue_() {
    assert((uintptr_t)&queue_ % HPX_CACHELINE_SIZE == 0);
    sync_two_lock_queue_init(&queue_, NULL);
  }

  ~TwoLockQueue() {
    sync_two_lock_queue_fini(&queue_);
  }

  T* dequeue() {
    return static_cast<T*>(sync_two_lock_queue_dequeue(&queue_));
  }

  void enqueue(T* t) {
    sync_two_lock_queue_enqueue(&queue_, t);
  }

 private:
  two_lock_queue_t queue_;
};

} // namespace util
} // namespace libhpx

#endif // LIBHPX_UTIL_TWO_LOCK_QUEUE_H
