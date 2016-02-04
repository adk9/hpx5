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
 
  public:
  
/// Fully synchronous call interface.
///
/// Performs action on @p args at @p addr, and sets @p out with the
/// resulting value. The output value @p out can be NULL, in which case no return
/// value is generated. The type of @p out must be the same as output type of the calling action.
///
/// @param         addr The address that defines where the action is executed.
/// @param       action The action to perform.
/// @param          out Reference to the output variable.
/// @param            n The number of arguments for @p action.
///
/// @returns            HPX_SUCCESS, or an error code if the action generated an
///                     error that could not be handled remotely.
  template <typename R, typename... Args>
  static int call_sync(hpx_addr_t& addr, R& out, Args&... args) {
    static_assert(!(std::is_same<void, R>::value or std::is_same<R, typename A::OType>::value), "output types do not match");
    static_assert(::std::is_same< typename A::traits::arg_types, hpx::detail::tlist<Args...> >::value,
                  "action and argument types do not match");
    return _hpx_call_sync(addr, A::id, &out, sizeof(R), sizeof...(Args), &args...);
  }

/// Locally synchronous call interface.
///
/// This is a locally-synchronous, globally-asynchronous variant of
/// the remote-procedure call interface. If @p result is not HPX_NULL,
/// hpx_call puts the the resulting value in @p result at some point
/// in the future.
///
/// @param         addr The address that defines where the action is executed.
/// @param       result An address of an LCO to trigger with the result.
///
/// @returns            HPX_SUCCESS, or an error code if there was a problem
///                     locally during the hpx_call invocation.
  template <typename... Args>
  static int call(hpx_addr_t addr, hpx_addr_t result, Args&... args) {
    return _hpx_call(addr, result, sizeof...(Args), &args...);
  }
  template <typename T1, typename LCO, typename... Args>
  static int call(const ::hpx::global_ptr<T1>& addr, const ::hpx::global_ptr<LCO>& result, Args&... args) {
    return _hpx_call(addr.get(), result.get(), sizeof...(Args), &args...);
  }

  
/// Locally synchronous call interface when LCO is set.
///
/// This is a locally-synchronous, globally-asynchronous variant of
/// the remote-procedure call interface which implements the hpx_parcel_send_
/// through() function. The gate must be non-HPX_NULL.
///
/// @param         gate The LCO that will serve as the gate (not HPX_NULL).
/// @param         addr The address that defines where the action is executed.
/// @param       result An address of an LCO to trigger with the result.
/// @param            n The number of arguments for @p action.
///
/// @returns            HPX_SUCCESS, or an error code if there was a problem
///                     locally during the hpx_call invocation.
  template <typename... Args>
  int call_when(hpx_addr_t gate, hpx_addr_t addr, hpx_addr_t result, Args&... args) {
    return _hpx_call_when(gate, addr, result, sizeof...(Args), &args...);
  }
  template <typename LCO, typename T1, typename T2, typename... Args>
  int call_when(const ::hpx::global_ptr<LCO>& gate, 
		const ::hpx::global_ptr<T1>& addr, 
		const ::hpx::global_ptr<T1>& result, Args&... args) {
    return _hpx_call_when(gate.get(), addr.get(), result.get(), sizeof...(Args), &args...);
  }
  
/// Locally synchronous call_when with continuation interface.
///
/// The gate must be non-HPX_NULL.
///
/// @param         gate The LCO that will serve as the gate (not HPX_NULL).
/// @param         addr The address that defines where the action is executed.
/// @param     c_target The address where the continuation action is executed.
/// @param         Cont The continuation action to perform.
///
/// @returns            HPX_SUCCESS, or an error code if there was a problem
///                     locally during the hpx_call invocation.
  template <typename Cont, typename...Args>
  int call_when_with_continuation(hpx_addr_t gate, hpx_addr_t addr, hpx_addr_t c_target, Args&... args) {
    return _hpx_call_when_with_continuation(gate, addr, A::id, c_target, Cont::id, sizeof...(Args), &args...);
  }
  template <typename LCO, typename T1, typename T2, typename Cont, typename...Args>
  int call_when_with_continuation(const ::hpx::global_ptr<LCO>& gate, 
				  const ::hpx::global_ptr<T1>& addr, 
				  const ::hpx::global_ptr<T2>& c_target, Args&... args) {
    return _hpx_call_when_with_continuation(gate.get(), addr.get(), A::id, c_target.get(), Cont::id, sizeof...(Args), &args...);
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

#define HPXPP_MAKE_ACTION(f, outtype, ...)				\
  struct f##_action_struct : public hpx::detail::action_struct<f##_action_struct> { \
    static hpx_action_t id;                                             \
    using OType = outtype;						\
    using traits = hpx::detail::function_traits<decltype(f)>;		\
  };                                                                    \
  hpx_action_t f##_action_struct::id = 0;                               \
  int f##_action_struct_dummy = f##_action_struct::_register(f);

#endif // HPX_CXX_ACTION_H
