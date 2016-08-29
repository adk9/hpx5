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

#ifndef LIBHPX_UTIL_CHASE_LEV_DEQUE_H
#define LIBHPX_UTIL_CHASE_LEV_DEQUE_H

#include "libhpx/util/Aligned.h"             // template Align
#include "libsync/deques.h"                  // chase_lev_ws_deque
#include "hpx/hpx.h"                         // HPX_CACHELINE_SIZE
#include <cassert>                           // assert
#include <cstdint>                           // uintptr_t

namespace libhpx {
namespace util {

/// Class representing a worker thread's state.
///
/// Worker threads are "object-oriented" insofar as that goes, but each native
/// thread has exactly one, thread-local worker structure, so the interface
/// doesn't take a "this" pointer and instead grabs the "self" structure using
/// __thread local storage.
///
/// @{
template <typename T>
class ChaseLevDeque : public Aligned<HPX_CACHELINE_SIZE>
{
 public:
  ChaseLevDeque() : work_() {
    assert((uintptr_t)&work_ % HPX_CACHELINE_SIZE == 0);
    sync_chase_lev_ws_deque_init(&work_, 32);
  }

  ~ChaseLevDeque() {
    sync_chase_lev_ws_deque_fini(&work_);
  }

  size_t size() const {
    return sync_chase_lev_ws_deque_size(&work_);
  }

  T* pop() {
    return static_cast<T*>(sync_chase_lev_ws_deque_pop(&work_));
  }

  T* steal() {
    return static_cast<T*>(sync_chase_lev_ws_deque_steal(&work_));
  }

  size_t push(T *p) {
    return sync_chase_lev_ws_deque_push(&work_, p);
  }

 private:
  chase_lev_ws_deque_t work_;
};

} // namespace util
} // namespace libhpx

#endif // LIBHPX_UTIL_CHASE_LEV_DEQUE_H
