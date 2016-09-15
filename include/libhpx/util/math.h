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

#ifndef HPX_UTIL_MATH_H
#define HPX_UTIL_MATH_H

namespace libhpx {
namespace util {
namespace detail {
template <typename T, size_t Bytes = sizeof(T)>
struct CeilLog2;

template <typename T>
struct CeilLog2<T, 4> {
  static constexpr T op(T val) {
    return ((sizeof(val) * 4 - 1) - __builtin_clz(val)) + (!!(val & (val - 1)));
  }
};

template <typename T>
struct CeilLog2<T, 8> {
  static constexpr T op(T val) {
    return ((sizeof(val) * 8 - 1) - __builtin_clzl(val)) + (!!(val & (val - 1)));
  }
};
}

template <typename T>
inline constexpr T ceil_log2(T val) {
  return detail::CeilLog2<T>::op(val);
}

template <typename T>
inline constexpr T ceil2(T val) {
  return T(1) << ceil_log2(val);
}

template <typename T>
static inline T ceil_div(T num, T denom) {
  return (num / denom) + ((num % denom) ? 1 : 0);
}
}
}

#endif // HPX_UTIL_MATH_H
