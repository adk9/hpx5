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
  
  template <typename T>
  class gp_subscr;
  
  /// An HPX global address ptr.
  /// Basically wraps hpx_addr_t and provides address arithmetic and pin/unpin methods
  template <typename T>
  class global_ptr {
    
  public:
    typedef T value_type;
    
    global_ptr() : _gbl_ptr(HPX_NULL), _elems_per_blk(1) {}
    global_ptr(hpx_addr_t addr, uint32_t b_s=1) : _gbl_ptr(addr), _elems_per_blk(b_s) {}
    
    /// return raw hpx_addr_t
    inline
    hpx_addr_t ptr() const {
      return _gbl_ptr;
    }
    
    inline
    uint32_t get_block_size() const {
      return _elems_per_blk;
    }
    
    /// pin and unpin
    T* pin() {
      T* ret;
      bool success = hpx_gas_try_pin(_gbl_ptr, (void**)&ret);
      if (!success) {
	// throw an exception?
	throw non_local_addr_exception();
	ret = nullptr;
      }
      return ret;
    }
    
    inline
    void unpin() {
      hpx_gas_unpin(_gbl_ptr);
    }
    
    /// returns true if the addr is local
    inline
    bool is_local() {
      return hpx_gas_try_pin(_gbl_ptr, NULL);
    }
    
    inline
    gp_subscr<T> operator[](size_t index) const;
  private:
    hpx_addr_t _gbl_ptr;
    size_t _elems_per_blk;
  };

  template <typename T>
  class gp_subscr {
  public:
    gp_subscr(const global_ptr<T>& gp, size_t offset) : _gp(gp), _offset(offset) {}
    
    inline
    global_ptr<T> operator&() const {
      return _gp + _offset;
    }
  private:
    global_ptr<T> _gp;
    size_t _offset;
  };
  
  template <typename T>
  inline
  gp_subscr<T> global_ptr<T>::operator[](size_t index) const {
    return gp_subscr<T>(*this, index);
  }
  
  namespace gas {
    template <typename T>
    inline
    global_ptr<T> alloc_local(size_t n, uint32_t boundary=0) {
      return global_ptr<T>(hpx_gas_alloc_local(n, sizeof(T), boundary));
    }
    
    template <typename T>
    inline
    global_ptr<T> alloc_cyclic(size_t total_elems, uint32_t elems_per_block=1, uint32_t boundary=0) {
      size_t n = total_elems / elems_per_block;
      return global_ptr<T>(hpx_gas_alloc_cyclic(n, elems_per_block * sizeof(T), boundary), elems_per_block);
    }
    
    template <typename T>
    inline
    global_ptr<T> alloc_blocked(size_t total_elems, uint32_t elems_per_block=1, uint32_t boundary=0) {
      size_t n = total_elems / elems_per_block;
      return global_ptr<T>(hpx_gas_alloc_blocked(n, elems_per_block * sizeof(T), boundary), elems_per_block);
    }
  }
  
}

template <typename T>
inline
hpx::global_ptr<T> operator+(const hpx::global_ptr<T>& lhs, size_t n) {
  return hpx::global_ptr<T>(hpx_addr_add(lhs.ptr(), n * sizeof(T), lhs.get_block_size() * sizeof(T)), lhs.get_block_size());
}

template <typename T>
inline
int64_t operator-(const hpx::global_ptr<T>& lhs, const hpx::global_ptr<T>& rhs) {
  return hpx_addr_sub(lhs.ptr(), rhs.ptr(), lhs.get_block_size() * sizeof(T));
}

#endif