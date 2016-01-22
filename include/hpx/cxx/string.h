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

#ifndef HPX_CXX_STRING_H
#define HPX_CXX_STRING_H

/// @file include/hpx/cxx/string.h
/// @brief Wrappers for the gas string functions (memget/memput/memcpy)

#include <hpx/cxx/global_ptr.h>

namespace hpx {
namespace gas {

/// The memget_sync wrappers.
///
/// These wrap the hpx_gas_memget operations, basically just allowing the client
/// to pass in a global pointer instead of hpx_addr_t. They don't try and type
/// check the @p to address (though one would expect it to be T*), and they
/// don't take @p n in terms of T elements, because that doesn't match the
/// semantics of memget.
///
/// @{

/// This version of memget is completely synchronous.
///
/// @param           to The local pointer we're copying to.
/// @param         from The global pointer we're copying from.
/// @param            n The number of bytes to copy
template <typename T>
void memget(void* to, global_ptr<T> from, size_t n) {
  if (int e = hpx_gas_memget_sync(to, from.ptr(), n)) {
    throw e;
  }
} // template memget

/// This version of memget is completely asynchronous, i.e., it will return
/// immediately.
///
/// @param           to The local pointer we're copying to.
/// @param         from The global pointer we're copying from.
/// @param            n The number of bytes to copy
template <typename T, typename U>
void memget(void* to, global_ptr<T> from, size_t n, global_ptr<U> lsync) {
  if (int e = hpx_gas_memget(to, from.ptr(), n, lsync.ptr())) {
    throw e;
  }
} // template memget<T, U>

/// @} memget


/// The memput wrappers.
///
/// These wrap the hpx_gas_memget operations, allowing the client to pass in
/// global pointers instead of hpx_addr_t. The don't try and type check the @p
/// from address (though one would expect it to be a T*), and they don't take @p
/// n in terms of T-typed elements, because that doesn't match the semantics of
/// memput.
///
/// @{

/// This version of memput is completely synchronous, it will not return until
/// the remote value has completed.
///
/// @param           to The global address we're putting to.
/// @param         from The local address we're putting from.
/// @param            n The number of bytes to put.
template <typename T>
void memput(global_ptr<T> to, const void *from, size_t n) {
  if (int e = hpx_gas_memput_rsync(to.ptr(), from, n)) {
    throw e;
  }
}

/// This version of memput is locally synchronous, it will return when the @p
/// from buffer can be modified or freed.
///
/// @param           to The global address we're putting to.
/// @param         from The local address we're putting from.
/// @param            n The number of bytes to put.
/// @param        rsync An LCO that will be set when the put is complete.
template <typename T, typename U>
void memput(global_ptr<T> to, const void *from, size_t n, global_ptr<U> rsync) {
  if (int e = hpx_gas_memput_lsync(to.ptr(), from, n, rsync.ptr())) {
    throw e;
  }
}

/// This version of memput is fully asynchronous.
///
/// @param           to The global address we're putting to.
/// @param         from The local address we're putting from.
/// @param            n The number of bytes to put.
/// @param        lsync An LCO that will be set when @p from can be modified.
/// @param        rsync An LCO that will be set when the put is complete.
template <typename T, typename U, typename V>
void memput(global_ptr<T> to, const void *from, size_t n, global_ptr<U> lsync,
            global_ptr<V> rsync) {
  if (int e = hpx_gas_memput(to.ptr(), from, n, lsync.ptr(), rsync.ptr())) {
    throw e;
  }
}

/// @}

} // namespace gas
} // namespace hpx

#endif // HPX_CXX_STRING_H
