// ================================================================= -*- C++ -*-
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

#ifndef HPX_CXX_LCO_H
#define HPX_CXX_LCO_H

#include <hpx/addr.h>
#include <hpx/lco.h>
#include <hpx/cxx/errors.h>
#include <hpx/cxx/global_ptr.h>

namespace hpx {
namespace lco {
/// This class is provides the lco interface common to all types of LCOs
/// extended by all LCO classes such as Future, Reduce, AndGate etc.
class LCO {
 protected:
  LCO() {}

  ~LCO() = delete;
};

template <typename T>
struct is_lco : public std::is_base_of<LCO, T> {
};

template <>
struct is_lco<void> {
  using value = std::integral_constant<bool, true>;
};

template <typename T>
void reset(const global_ptr<T>& lco) {
  static_assert(is_lco<T>::value, "LCO type required");
  hpx_lco_reset(lco.get());
}

template <typename T>
void wait(const global_ptr<T>& lco) {
  static_assert(is_lco<T>::value, "LCO type required");
  if (int e = hpx_lco_wait(lco.get())) {
    throw Error(e);
  }
}

template <typename T, typename U>
void get(const global_ptr<T>& lco, U& out) {
  static_assert(is_lco<T>::value, "LCO type required");
  if (int e = hpx_lco_get(lco.get(), sizeof(out), &out)) {
    throw Error(e);
  }
}

template <typename T, template <typename> class LCO>
T get(const global_ptr<LCO<T>>& lco) {
  T out;
  if (int e = hpx_lco_get(lco.get(), sizeof(out), &out)) {
    throw Error(e);
  }
  return out;
}

template <template <typename> class LCO>
void get(const global_ptr<LCO<void>>& lco) {
  if (int e = hpx_lco_wait(lco.get())) {
    throw Error(e);
  }
}

template <typename T>
void get(const global_ptr<T>& lco) {
  wait(lco);
}

template <typename T, typename U>
void set(const global_ptr<T>& lco, U&& in) {
  static_assert(is_lco<T>::value, "LCO type required");
  hpx_lco_set_rsync(lco.get(), sizeof(in), std::forward<U>(in));
}

template <typename T>
void dealloc(const global_ptr<T>& lco) {
  static_assert(is_lco<T>::value, "LCO type required");
  hpx_lco_delete_sync(lco.get());
}

template <typename T, typename U>
void dealloc(const global_ptr<T>& lco, const global_ptr<U>& sync) {
  static_assert(is_lco<T>::value, "LCO type required");
  static_assert(is_lco<T>::value, "LCO type required");
  hpx_lco_delete(lco.get(), sync.get());
}

template <typename T>
void dealloc(const global_ptr<T>& lco, std::nullptr_t) {
  static_assert(is_lco<T>::value, "LCO type required");
  hpx_lco_delete_sync(lco.get(), HPX_NULL);
}

template <typename T>
class Future : public LCO {
 public:
  Future() = delete;
  ~Future() = delete;

  static global_ptr<Future<T>> Alloc() {
    return global_ptr<Future<T>>(hpx_lco_future_new(sizeof(T)));
  }
};

/// Future to void is a 0-sized future that only contains control information.
template <>
global_ptr<Future<void>> Future<void>::Alloc() {
  return global_ptr<Future<void>>(hpx_lco_future_new(0));
}

class And : public LCO {
 public:
  And() = delete;
  ~And() = delete;

  static global_ptr<And> Alloc(size_t n) {
    return global_ptr<And>(hpx_lco_and_new(n));
  }
};

template <typename T>
class Reduce : public LCO {
 public:
  Reduce() = delete;
  ~Reduce() = delete;

  template <template <typename> class Op>
  static global_ptr<Reduce<T>> Alloc(int inputs) {
    hpx_action_t id = Op<T>::id;
    hpx_action_t op = Op<T>::op;
    return global_ptr<Reduce<T>>(hpx_lco_reduce_new(inputs, sizeof(T), id, op));
  }
};

namespace Ops {
template <typename T>
class Sum {
  static void init(T* t, size_t) {
    *t = T(0);
  }

  static void sum(T* lhs, const T* rhs, size_t) {
    *lhs += *rhs;
  }

 public:
  static int Init() {
    if (int e = hpx_register_action(HPX_FUNCTION, 0, "SumId", &id, 1, &init)) {
      throw Error(e);
    }

    if (int e = hpx_register_action(HPX_FUNCTION, 0, "SumOp", &op, 1, &sum)) {
      throw Error(e);
    }

    return HPX_SUCCESS;
  }

  static hpx_action_t id;
  static hpx_action_t op;
};

template <typename T>
class Product {
  static void init(T* t, size_t) {
    *t = T(1);
  }

  static void product(T* lhs, const T* rhs, size_t) {
    *lhs *= *rhs;
  }

 public:
  static int Init() {
    if (int e = hpx_register_action(HPX_FUNCTION, 0, "", &id, 1, &init)) {
      throw Error(e);
    }

    if (int e = hpx_register_action(HPX_FUNCTION, 0, "", &op, 1, &product)) {
      throw Error(e);
    }

    return HPX_SUCCESS;
  }

  static hpx_action_t id;
  static hpx_action_t op;
};

template <typename T> hpx_action_t Sum<T>::id = HPX_ACTION_NULL;
template <typename T> hpx_action_t Sum<T>::op = HPX_ACTION_NULL;
template <typename T> hpx_action_t Product<T>::id = HPX_ACTION_NULL;
template <typename T> hpx_action_t Product<T>::op = HPX_ACTION_NULL;
} // namespace ops
} // namespace lco
} // namespace hpx

#endif // HPX_CXX_LCO_H
