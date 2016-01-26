// ================================================================= -*- C++ -*-
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================

#ifndef HPX_CXX_ACTION_H
#define HPX_CXX_ACTION_H

#include <type_traits>

#include <hpx/addr.h>
#include <hpx/types.h>
#include <hpx/action.h>
#include <hpx/rpc.h>
#include <hpx/cxx/lco.h>
#include <hpx/cxx/runtime.h>

namespace hpx {

namespace detail {
  
// used to check sameness of type lists
template <typename... Ts>
struct tlist {};
  
// reference: https://functionalcpp.wordpress.com/2013/08/05/function-traits/
template<class F>
struct function_traits;

// function pointer
template<class R, class... Args>
struct function_traits<R(*)(Args...)> : public function_traits<R(Args...)>
{};

template<class R, class... Args>
struct function_traits<R(&)(Args...)> : public function_traits<R(Args...)>
{};

template<class R, class... Args>
struct function_traits<R(Args...)> {
  using return_type = R;
  static constexpr std::size_t arity = sizeof...(Args);
  using arg_types = tlist<Args...>;
}; // template struct function_traits

/// HPX basic datatypes
/// HPX_CHAR, HPX_UCHAR, HPX_SCHAR, HPX_SHORT, HPX_USHORT, HPX_SSHORT, HPX_INT,
/// HPX_UINT, HPX_SINT, HPX_LONG, HPX_ULONG, HPX_SLONG, HPX_VOID, HPX_UINT8,
/// HPX_SINT8, HPX_UINT16, HPX_SINT16, HPX_UINT32, HPX_SINT32, HPX_UINT64,
/// HPX_SINT64, HPX_FLOAT, HPX_DOUBLE, HPX_POINTER, HPX_LONGDOUBLE,
/// HPX_COMPLEX_FLOAT, HPX_COMPLEX_DOUBLE, HPX_COMPLEX_LONGDOUBLE

template <typename T>
struct _convert_arg_type;

#define DEF_CONVERT_TYPE(cpptype, hpxtype)			\
template<> struct _convert_arg_type<cpptype> {			\
  constexpr static auto type = hpxtype;				\
};								\
constexpr decltype(hpxtype) _convert_arg_type<cpptype>::type;

DEF_CONVERT_TYPE(char, HPX_CHAR)
DEF_CONVERT_TYPE(short, HPX_SHORT)
DEF_CONVERT_TYPE(int, HPX_INT)
DEF_CONVERT_TYPE(float, HPX_FLOAT)
DEF_CONVERT_TYPE(double, HPX_DOUBLE)
DEF_CONVERT_TYPE(std::size_t, HPX_SIZE_T)

template <typename A>
class action_struct {
 private:
  /// @param        type The type of the action (THREAD, TASK, INTERRUPT, ...).
  /// @param        attr The attribute of the action (PINNED, PACKED, ...).
  /// @param     handler The action handler (the function).
  /// @param          id The action id (the hpx_action_t address).
  /// @param __VA_ARGS__ The parameter types (HPX_INT, ...).
  /// #define HPX_REGISTER_ACTION(type, attr, id, handler, ...)

 public:
  
  template <typename R, typename... Args>
  static int call_sync(hpx_addr_t& addr, R& result, Args&... args) {
    static_assert(::std::is_same< typename A::traits::arg_types, hpx::detail::tlist<Args...> >::value,
                  "action and argument types do not match");
    return _hpx_call_sync(addr, A::id, &result, sizeof(R), sizeof...(Args), &args...);
  }

  template <typename... Args>
  static int run(Args&... args) {
    static_assert(::std::is_same< typename A::traits::arg_types, hpx::detail::tlist<Args...> >::value,
                  "action and argument types do not match");
    return hpx::run(&(A::id), &args...);
  }
  
  template <typename R, typename... Args>
  static int _register(R(&f)(Args...)) {
    return hpx_register_action(HPX_DEFAULT, HPX_ATTR_NONE, __FILE__ ":" _HPX_XSTR(A::id),
                               &(A::id), sizeof...(Args) + 1 , f,
                               _convert_arg_type<Args>::type...);
  }
  
}; // template class action_struct

} // namespace detail
} // namspace hpx

#define HPXPP_REGISTER_ACTION(f)                                        \
  struct f##_action_struct : public hpx::detail::action_struct<f##_action_struct> { \
    static hpx_action_t id;                                             \
    using traits = hpx::detail::function_traits<decltype(f)>;		\
  };                                                                    \
  hpx_action_t f##_action_struct::id = 0;                               \
  int f##_action_struct_dummy = f##_action_struct::_register(f);

#endif // HPX_CXX_ACTION_H
