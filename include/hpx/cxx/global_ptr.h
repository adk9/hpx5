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

#ifndef HPX_CXX_GLOBAL_PTR_H
#define HPX_CXX_GLOBAL_PTR_H

#include <memory>
#include <exception>
#include <hpx/addr.h>
#include <hpx/gas.h>

namespace hpx {

template <typename T>
class gp_subscr;

/// An HPX global address ptr.
///
/// Basically wraps hpx_addr_t and provides address arithmetic and pin/unpin
/// methods.
template <typename T>
class global_ptr {

 public:
  typedef T value_type;

  global_ptr() : _gbl_ptr(HPX_NULL), _elems_per_blk(0) {
  }

  global_ptr(hpx_addr_t addr) : _gbl_ptr(addr), _elems_per_blk(1) {
  }

  global_ptr(hpx_addr_t addr, uint32_t b_s) : _gbl_ptr(addr),
                                              _elems_per_blk(b_s) {
  }

  /// return raw hpx_addr_t
  hpx_addr_t ptr() const {
    return _gbl_ptr;
  }

  uint32_t get_block_size() const {
    return _elems_per_blk;
  }

  /// returns true if the addr is local
  bool is_local() const {
    return hpx_gas_try_pin(_gbl_ptr, NULL);
  }

  inline
  gp_subscr<T> operator[](size_t index) const;

  global_ptr<T> operator+(ptrdiff_t n) const {
    ptrdiff_t bytes = n * sizeof(T);
    hpx_addr_t addr = hpx_addr_add(_gbl_ptr, bytes, bsize());
    return global_ptr<T>(addr, _elems_per_blk);
  }

  ptrdiff_t operator-(const global_ptr<T>& rhs) const {
    assert(_elems_per_blk == rhs.get_block_size());
    return hpx_addr_sub(_gbl_ptr, rhs.ptr(), bsize());
  }

 protected:
  size_t bsize() const {
    return _elems_per_blk * sizeof(T);
  }

 private:
  hpx_addr_t _gbl_ptr;
  size_t _elems_per_blk;
}; // template class global_ptr

template <typename T>
class pinned_ptr {
 public:

  /// We throw this exception if pinning the operation fails.
  struct Failed : public std::exception {
    virtual const char* what() const throw() {
      return "Pin operation failed\n";
    }
  };

  /// Construct a pinned pointer.
  ///
  /// A pinned pointer will automatically unpin the address when it is
  /// destroyed.
  ///
  /// This constructor version will throw a PinFailed() exception if the pin
  /// operation fails. We generally recommend using the version that takes the
  /// local flag as output.
  ///
  /// @param       addr The address we're trying to pin.
  explicit pinned_ptr(global_ptr<T> &gva)
    : _addr(gva.ptr()),
      _lva(NULL),
      _local(true) {
    if (!hpx_gas_try_pin(_addr, reinterpret_cast<void**>(&_lva))) {
      throw Failed();
    }
  }

  /// Construct a pinned pointer.
  ///
  /// A pinned pointer will automatically unpin the address when it is
  /// destroyed.
  ///
  /// This constructor version will return its success/failure result through
  /// the @p local flag.
  pinned_ptr(global_ptr<T> &gva, bool& local)
    : _addr(gva.ptr()),
      _lva(NULL),
      _local(hpx_gas_try_pin(_addr, reinterpret_cast<void**>(&_lva))) {
    local = _local;
  }

  /// Release the address if it was pinned.
  ~pinned_ptr() {
    if (_local) {
      hpx_gas_unpin(_addr);
    }
  }


  /// Cast to a T*.
  ///
  /// The pinned pointer's lifetime *must* exceed that of the pointer. Using the
  /// returned pointer once the object has been destroyed is an error.
  T* unsafe_impl() const {
    return _lva;
  }

  /// Standard smart pointer operation.
  T* operator->() const {
    return _lva;
  }

  /// Standard smart pointer operation.
  T& operator*() const {
    return *_lva;
  }

 private:
  pinned_ptr();
  pinned_ptr(const pinned_ptr<T>&);
  pinned_ptr<T>& operator=(const pinned_ptr<T>&);

  hpx_addr_t _addr;
  T*          _lva;
  bool      _local;
};

/// Disable pinned pointers to void*.
template <>
class pinned_ptr<void> {
 private:
  pinned_ptr();
  pinned_ptr(const pinned_ptr&);
  pinned_ptr& operator=(const pinned_ptr&);
  ~pinned_ptr();
};

/// Special case global pointers to void with a template specialization.
///
/// These serve the same roll as void* pointers do. Other pointers can be cast
/// to global_ptr<void> and global_ptr<void> will be cast to other pointers,
/// however we can't pin, unpin, or perform address arithmetic on them.
template <>
class global_ptr<void> {
 public:
  /// Default constructor.
  ///
  /// This constructs a global_ptr<void> to NULL.
  global_ptr<void>() : _impl(0), _bsize(0) {
  }

  /// Construct a generic global pointer from a generic HPX address and block
  /// size.
  global_ptr<void>(hpx_addr_t addr, size_t bsize) : _impl(addr), _bsize(bsize) {
  }

  /// Construct a generic global pointer from a pointer to any other type---this
  /// serves to handle pointer casts as well.
  template <typename U>
  global_ptr<void>(const global_ptr<U>& ptr) : _impl(ptr.ptr()),
    _bsize(ptr.get_block_size()) {
  }

  /// Implicitly cast back to a typed smart pointer.
  template <typename U>
  operator global_ptr<U>() {
    return global_ptr<U>(_impl, _bsize);
  }

  /// Support any user that wants to get the underlying HPX address.
  hpx_addr_t ptr() const {
    return _impl;
  }

  /// Support any user that wants to get the underlying HPX block size.
  size_t get_block_size() const {
    return _bsize;
  }

 private:
  hpx_addr_t _impl;
  size_t _bsize;
};

template <typename T, typename U>
bool operator==(const global_ptr<T>& lhs, const global_ptr<U> &rhs) {
  return ((lhs - rhs) == 0);
}

template <typename T, typename U>
bool operator!=(const global_ptr<T>& lhs, const global_ptr<U> &rhs) {
  return !(lhs == rhs);
}

template <typename T, typename U>
bool operator<(const global_ptr<T>& lhs, const global_ptr<U> &rhs) {
  assert(lhs.get_block_size() == rhs.get_block_size());
  return ((lhs - rhs) < 0);
}

template <typename T, typename U>
bool operator<=(const global_ptr<T>& lhs, const global_ptr<U> &rhs) {
  assert(lhs.get_block_size() == rhs.get_block_size());
  return ((lhs - rhs) <= 0);
}

template <typename T, typename U>
bool operator>(const global_ptr<T>& lhs, const global_ptr<U> &rhs) {
  assert(lhs.get_block_size() == rhs.get_block_size());
  return ((lhs - rhs) > 0);
}

template <typename T, typename U>
bool operator>=(const global_ptr<T>& lhs, const global_ptr<U> &rhs) {
  assert(lhs.get_block_size() == rhs.get_block_size());
  return ((lhs - rhs) >= 0);
}

/// The NULL pointer can be represented as the default global pointer.
/// @{
const global_ptr<void> null_ptr;
/// @}

template <typename T>
class gp_subscr {
 public:
  gp_subscr(const global_ptr<T>& gp, size_t offset) : _gp(gp), _offset(offset) {}

  global_ptr<T> operator&() const;

 private:
  global_ptr<T> _gp;
  size_t _offset;
}; // template class gp_subscr

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
} // namespace gas
} // namespace hpx

namespace hpx {
template <typename T>
inline
global_ptr<T> gp_subscr<T>::operator&() const {
  return _gp + _offset;
}

} // namespace hpx

#endif // HPX_CXX_GLOBAL_PTR_H
