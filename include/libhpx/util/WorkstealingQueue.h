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

#ifndef LIBHPX_UTIL_WORKSTEALING_QUEUE_H
#define LIBHPX_UTIL_WORKSTEALING_QUEUE_H

#include "libhpx/util/math.h"
#include <climits>
#include <cstdint>
#include <atomic>

namespace libhpx {
namespace util {

template <typename T>
class WorkstealingQueue;

template <typename T>
class WorkstealingQueue<T*> {
  using Index = std::uint_fast64_t;

  // This class represents a Bounded Buffer used in the workstealing
  // implementation. It maps indexes into buffer slots.
  class Buffer {
   public:
    Buffer(unsigned size, Index last, Index next, Buffer* parent) :
        mask_(Index(size) - 1),
        parent_(parent),
        data_(new T*[size]) {
      assert(!last || parent_);
      for (auto i = last; i < next; ++i) {
        set(i, parent_->get(i));
      }
    }

    ~Buffer() {
      delete [] data_;
      delete parent_;
    }

    T*& get(Index i) const {
      return data_[i & mask_];
    }

    void set(Index i, T* t) {
      data_[i & mask_] = t;
    }

   private:
    const Index mask_;
    Buffer* parent_;
    T** data_;
  };

 public:
  WorkstealingQueue(unsigned size) :
      capacity_(std::max(unsigned(1) << ceil_log2(size), 1u)),
      next_(0),
      last_(0),
      buffer_(new Buffer(capacity_, 0, 0, nullptr))
  {
  }

  ~WorkstealingQueue() {
  }

  /// Push a pointer into the queue.
  ///
  /// This will store the pointer into the queue. It will return an approximate
  /// number of elements in the queue.
  unsigned push(T* t) {
    auto next = next_.load(std::memory_order_relaxed);
    auto last = last_.load(std::memory_order_seq_cst);
    assert(next - last < UINT_MAX);
    unsigned size = unsigned(next - last);
    if (size == capacity_) {
      capacity_ *= 2;
      assert(size < capacity_);
      buffer_ = new Buffer(capacity_, last, next, buffer_);
    }
    buffer_->set(next, t);
    next_.store(next + 1, std::memory_order_relaxed);
    return size;
  }

  /// Try and pop a pointer from the queue.
  T* pop() {
    auto next = next_.load(std::memory_order_relaxed);
    auto last = last_.fetch_add(1, std::memory_order_seq_cst);
    if (last == next) {
      last_.store(next, std::memory_order_seq_cst);
      return nullptr;
    }
    else {
      assert(last < next);
      return buffer_->get(last);
    }
  }

  /// Try and steal a number of pointers from the queue.
  template <typename Lambda>
  T* steal(unsigned n, Lambda&& lambda) {
    // See if it looks like there are any pointers in this queue.
    auto next = next_.load(std::memory_order_relaxed);
    auto last = last_.load(std::memory_order_seq_cst);
    if (last >= next) {
      return nullptr;
    }

    // Read out N pointers onto the stack.
    auto N = std::min(n, unsigned(next - last));
    T* it = static_cast<T*>(alloca(N * sizeof(T*)));
    for (auto i = 0; i < N; ++i) {
      it[i] = buffer_->get(last + i);
    }

    // If the last pointer changed then we failed to steal.
    if (!last_.compare_exchange_weak(last, last + N)) {
      return nullptr;
    }

    // Otherwise use the lambda function to read out all but one of the pointers
    // we stole and then return the oldest one
    for (auto i = 1; i < N; ++i) {
      lamdba(it[i]);
    }
    return it[0];
  }

  T* steal() {
    // See if it looks like there are any pointers in this queue.
    auto next = next_.load(std::memory_order_relaxed);
    auto last = last_.load(std::memory_order_seq_cst);
    if (last >= next) {
      return nullptr;
    }

    // Read out N pointers onto the stack.
    T* t = buffer_->get(last);
    return (last_.compare_exchange_weak(last, last + 1)) ? t : nullptr;
  }

  unsigned size() const {
    auto next = next_.load(std::memory_order_relaxed);
    auto last = last_.load(std::memory_order_seq_cst);
    assert(next - last < UINT_MAX);
    return unsigned(next - last);
  }

 private:
  unsigned capacity_;
  std::atomic<Index> next_;
  std::atomic<Index> last_;
  Buffer *buffer_;
};

} // namespace util
} // namespace libhpx

#endif //  LIBHPX_UTIL_WORKSTEALING_QUEUE_H
