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
#ifndef LIBHPX_SYNC_GCC_BUILTIN_SYNC_H_
#define LIBHPX_SYNC_GCC_BUILTIN_SYNC_H_

/*
  ====================================================================
  Defines our synchronization interface in terms of the older gcc
  __sync builtins defined in
  http://gcc.gnu.org/onlinedocs/gcc/_005f_005fsync-Builtins.html#g_t_005f_005fsync-Builtins
  ====================================================================
*/

/* no memory model */
#define HPX_SYNC_RELAXED
#define HPX_SYNC_CONSUME
#define HPX_SYNC_ACQ_REL
#define HPX_SYNC_SEQ_CST
#define HPX_SYNC_ACQUIRE
#define HPX_SYNC_RELEASE

/*
 * I don't have a great way to implement an atomic load using macros, given the
 * interface that we're dealign with. We'll probably have to extend the
 * interface to take a type or something awkward...
 *
 * Not currently used in source, so we'll deal with it when we need it.
 */
/* #define hpx_sync_load(addr, mm) *addr */

/*
 * Synchronizing a store requires that all previous operations complete before
 * the store occurs. Normal TSO (and x86-TSO) provides this in hardware (the
 * store won't bypass previous loads or stores), so we just need to make sure
 * that the compiler understands not to reorder the store with previous
 * operations.
 */
#define hpx_sync_store(addr, val, mm) do {      \
    __asm volatile ("":::"memory");             \
    *addr = val;                                \
  } while (0)

#define hpx_sync_swap(addr, val, mm) __sync_lock_test_and_set (addr, val)

#define hpx_sync_cas(addr, from, to, onsuccess, onfailure)  \
  __sync_bool_compare_and_swap(addr, from, to)

#define hpx_sync_cas_val(addr, from, to, onsuccess, onfailure)  \
  __sync_val_compare_and_swap(addr, from, to)

#define hpx_sync_fadd(addr, val, mm) __sync_fetch_and_add(addr, val)

#define hpx_sync_fence(mm) __sync_synchronize()

/*
  ====================================================================
  buitin_common.h implements all of the strongly-typed versions in
  terms of the above generic versions.
  ====================================================================
*/

#endif /* LIBHPX_SYNC_GCC_BUILTIN_SYNC_H_ */
