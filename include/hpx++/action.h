// =============================================================================
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

#ifndef ACTION_H
#define ACTION_H

extern "C" {
  #include "hpx/addr.h"
  #include "hpx/types.h"
  #include "hpx/action.h"
  #include "hpx/rpc.h"
}

#include <hpx++/lco.h>

#include <cstdlib>
#include <tuple>
#include <type_traits>

namespace hpx {
  
  namespace detail {
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
	using arg_types = std::tuple<Args...>;
	static constexpr auto arg_types_tpl = std::tuple<Args...>();
    };
    
    template <::std::size_t I, typename FT, typename... Args>
    struct matching_helper {
      using f_type = typename ::std::tuple_element< I, typename FT::arg_types >::type;
      using s_type = typename ::std::tuple_element< I, std::tuple<Args...> >::type;
      static constexpr bool value = ::std::is_same< f_type, s_type >::value;
    };

    template <typename FT, typename... Args>
    struct is_matching {
      static constexpr bool value = (FT::arity == sizeof...(Args)) && matching_helper<0, FT, Args...>::value;
    };
    
    // reference: http://stackoverflow.com/questions/17424477/implementation-c14-make-integer-sequence
    // using aliases for cleaner syntax
    template<class T> using Invoke = typename T::type;

    template<unsigned...> struct seq{ using type = seq; };

    template<class S1, class S2> struct concat;

    template<unsigned... I1, unsigned... I2>
    struct concat<seq<I1...>, seq<I2...>>
      : seq<I1..., (sizeof...(I1)+I2)...>{};

    template<class S1, class S2>
    using Concat = Invoke<concat<S1, S2>>;

    template<unsigned N> struct gen_seq;
    template<unsigned N> using GenSeq = Invoke<gen_seq<N>>;

    template<unsigned N>
    struct gen_seq : Concat< GenSeq<N/2>, GenSeq<N - N/2> >{};

    template<> struct gen_seq<0> : seq<>{};
    template<> struct gen_seq<1> : seq<0>{};
    
    template <typename T>
    inline
    ::std::tuple<decltype(HPX_POINTER), decltype(HPX_SIZE_T)>
    xform(T&& t) {
      return ::std::make_tuple(HPX_POINTER, HPX_SIZE_T);
    }
    
    template <typename A>
    struct action_struct {
      /*
      * 
      /// @param        type The type of the action (THREAD, TASK, INTERRUPT, ...).
      /// @param        attr The attribute of the action (PINNED, PACKED, ...).
      /// @param     handler The action handler (the function).
      /// @param          id The action id (the hpx_action_t address).
      /// @param __VA_ARGS__ The parameter types (HPX_INT, ...).
      #define HPX_REGISTER_ACTION(type, attr, id, handler, ...)
      */
      // for 0 args, HPX_REGISTER_ACTION is not HPX_MARSHALLED
      template <typename F, typename Tpl>
      static int _register_helper(F f, Tpl&& t, seq<>&& s) {
	return HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_ATTR_NONE, A::id, f);
      }
      template <typename F, typename Tpl, unsigned... Is>
      static int _register_helper(F f, Tpl&& t, seq<Is...>&& s) {
	return HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, A::id, f, ::std::get<Is>(t)...);
      }
      
      template <typename F, unsigned... Is>
      static int _register(F f, seq<Is...>&& s) {
	auto tpl = ::std::tuple_cat(xform(::std::get<Is>(A::traits::arg_types_tpl))...);
	return _register_helper(f, tpl, gen_seq<::std::tuple_size<decltype(tpl)>::value>());
      }
      
//       template <typename... Args>
//       typename ::std::enable_if< ::std::is_void<typename A::traits::return_type>::value, int >::type
//       operator()(hpx_addr_t addr, Args... args) {
// 	return hpx_call_sync(addr, A::id, HPX_NULL, 0, args...);
//       }
    };
    
    template <typename T, typename F>
    inline
    int _register_action(F f) {
      return T::_register(f, hpx::detail::gen_seq<::std::tuple_size<decltype(T::traits::arg_types_tpl)>::value>());
    }
    
  }
  
  /*
   * int    _hpx_call(hpx_addr_t addr, hpx_action_t action, hpx_addr_t result,
                 int nargs, ...) HPX_PUBLIC;
     #define hpx_call(addr, action, result, ...) \
	_hpx_call(addr, action, result, __HPX_NARGS(__VA_ARGS__) , ##__VA_ARGS__)
   */
  template <typename F, typename RType, typename... Args>
  int call_sync(hpx_addr_t addr, F && f, RType& result, Args... args) {
    static_assert(hpx::detail::is_matching<hpx::detail::function_traits<decltype(f)>, Args...>::value, 
		  "action and argument types do not match");
    // TODO make lco out of result and pass that here
    return hpx_call_sync(addr, f, HPX_NULL, args...);
  }
}

#define HPXPP_REGISTER_ACTION(f)						\
struct f##_action_struct : public hpx::detail::action_struct<f##_action_struct> {\
  static hpx_action_t id;							\
  using traits = hpx::detail::function_traits<decltype(f)>;			\
  \
  \
  template <typename... Args>							\
  int operator()(hpx_addr_t addr, traits::return_type& result, Args... args) {	\
    std::cout << traits::arity << ", " << sizeof...(Args) << std::endl;\
    static_assert(hpx::detail::is_matching<traits, Args...>::value, \
		  "action and argument types do not match");\
    return hpx_call_sync(addr, id, &result, sizeof(traits::return_type), args...);\
  }										\
};										\
hpx_action_t f##_action_struct::id = 0;						\
int f##_action_struct_dummy = hpx::detail::_register_action<f##_action_struct>(f);

#endif