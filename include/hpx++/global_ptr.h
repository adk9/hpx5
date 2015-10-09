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

#ifndef GLOBAL_PTR_H
#define GLOBAL_PTR_H

#include <memory>
#include <exception>

extern "C" {
  #include "hpx/addr.h"
  #include "hpx/gas.h"
}
  
namespace hpx {
  
  /// This is thrown if pin on global_ptr is unsuccessful
  struct non_local_addr_exception : public std::exception {
    virtual const char* what() const throw() {
      return "Pinning non local address not allowed";
    }
  };
  
  /// An HPX global address ptr.
  /// Basically wraps hpx_addr_t and provides address arithmetic and pin/unpin methods
  template <typename T, size_t B=sizeof(T)>
  class global_ptr {
    
  public:
    typedef T value_type;
    
    global_ptr() : _gbl_ptr(HPX_NULL), _n(0) {}
    global_ptr(hpx_addr_t addr, size_t n) : _gbl_ptr(addr), _n(n) {}
    
    inline
    hpx_addr_t ptr() const {
      return _gbl_ptr;
    }
    
    // pin and unpin
    T* pin() {
      T* ret = new T[_n];
      bool success = hpx_gas_try_pin(_gbl_ptr, &ret);
      if (!success) {
	// throw an exception?
	throw non_local_addr_exception();
	ret = nullptr;
      }
      return ret;
    }
    
    inline
    void unpin() {
      unpin(_gbl_ptr);
    }
    
    // casting stuff
//     template <typename T, typename B2>
//     global_ptr<T, B>&
//     operator=(const global_ptr<T, B2>& rhs) {
//       this->n = rhs.n;
//       this->blk_sz = rhs.blk_sz;
//       return *this;
//     }
  private:
    hpx_addr_t _gbl_ptr;
    size_t _n;
  };

  namespace gas {
    template <typename T>
    inline
    global_ptr<T> alloc_local(size_t n, uint32_t boundary=0) {
      return global_ptr<T>(hpx_gas_alloc_local(n * sizeof(T), boundary), sizeof(T));
    }
    
    template <typename T, size_t B=sizeof(T)>
    inline
    global_ptr<T, B> alloc_cyclic(size_t n, uint32_t boundary=0) {
      return global_ptr<T, B>(hpx_gas_alloc_cyclic(n * sizeof(T), B, boundary), sizeof(T));
    }
    
    template <typename T, size_t B=sizeof(T)>
    inline
    global_ptr<T, B> alloc_blocked(size_t n, uint32_t boundary=0) {
      return global_ptr<T, B>(hpx_gas_alloc_blocked(n * sizeof(T), B, boundary), sizeof(T));
    }
  }
  
}

template <typename T, size_t B>
inline
hpx::global_ptr<T, B> operator+(const hpx::global_ptr<T, B>& lhs, size_t n) {
  return hpx::global_ptr<T>(hpx_addr_add(lhs.ptr(), n * sizeof(T), B), n);
}

template <typename T, size_t B>
inline
int64_t operator-(const hpx::global_ptr<T, B>& lhs, const hpx::global_ptr<T, B>& rhs) {
  return hpx_addr_sub(lhs.ptr(), rhs.ptr(), B);
}

#endif