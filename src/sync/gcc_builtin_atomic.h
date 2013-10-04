/*
  ====================================================================
  High Performance ParalleX Library (libhpx)

  Copyright (c) 2013, Trustees of Indiana University
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
  ====================================================================
*/

#pragma once
#ifndef LIBHPX_SYNC_GCC_BUILTIN_ATOMIC_H_
#define LIBHPX_SYNC_GCC_BUILTIN_ATOMIC_H_

/*
  ====================================================================
  The gcc-builtin atomic mapping. Just uses the builtins from
  http://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html
  directly.
  ====================================================================
*/

#define HPX_SYNC_RELAXED __ATOMIC_RELAXED
#define HPX_SYNC_CONSUME __ATOMIC_CONSUME
#define HPX_SYNC_ACQ_REL __ATOMIC_ACQ_REL
#define HPX_SYNC_SEQ_CST __ATOMIC_SEQ_CST

#ifdef __ATOMIC_HLE_ACQUIRE
#define HPX_SYNC_ACQUIRE __ATOMIC_ACQUIRE | __ATOMIC_HLE_ACQUIRE
#else
#define HPX_SYNC_ACQUIRE __ATOMIC_ACQUIRE
#endif

#ifdef __ATOMIC_HLE_RELEASE
#define HPX_SYNC_RELEASE __ATOMIC_RELEASE | __ATOMIC_HLE_RELEASE
#else
#define HPX_SYNC_RELEASE __ATOMIC_RELEASE
#endif

#define hpx_sync_load(addr, mm) __atomic_load_n(addr, mm)
#define hpx_sync_store(addr, val, mm) __atomic_store_n(addr, val, mm)
#define hpx_sync_swap(addr, val, mm) __atomic_exchange_n(addr, val, mm)
#define hpx_sync_cas(addr, from, to, onsuccess, onfailure)              \
    __atomic_compare_exchange_n(addr, from, to, false, onsuccess, onfailure)
#define hpx_sync_cas_val(addr, from, to, onsuccess, onfailure)          \
    { __atomic_compare_exchange_n(addr, from, to, false, onsuccess, onfailure) ? from : to }
#define hpx_sync_fadd(addr, val, mm) __atomic_fetch_add(addr, val, mm)
#define hpx_sync_fence(mm) __atomic_thread_fence(mm)

/*
  ====================================================================
  buitin_common.h implements all of the strongly-typed versions in
  terms of the above generic versions.
  ====================================================================
*/

#include "builtin_common.h"

#endif /* LIBHPX_SYNC_GCC_BUILTIN_ATOMIC_H_ */
