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
#ifndef HPX_SYNC_GCC_SYNC_H_
#define HPX_SYNC_GCC_SYNC_H_

/*
  ====================================================================
  Defines our synchronization interface in terms of the older gcc
  __sync builtins defined in
  http://gcc.gnu.org/onlinedocs/gcc/_005f_005fsync-Builtins.html#g_t_005f_005fsync-Builtins
  ====================================================================
*/

/* no memory model */
#define SYNC_RELAXED 0
#define SYNC_CONSUME 0
#define SYNC_ACQ_REL 0
#define SYNC_SEQ_CST 0
#define SYNC_ACQUIRE 0
#define SYNC_RELEASE 0

/*
 * Extremely annoying that val is required here, but without using GNU
 * extensions I can't figure out a good way to deal with the compiler barrier.
 */
#define sync_load(val, addr, mm) do {           \
    __asm volatile ("":::"memory");             \
    val = *addr;                                \
  } while (0)

/*
 * Synchronizing a store requires that all previous operations complete before
 * the store occurs. Normal TSO (and x86-TSO) provides this in hardware (the
 * store won't bypass previous loads or stores), so we just need to make sure
 * that the compiler understands not to reorder the store with previous
 * operations.
 */
#define sync_store(addr, val, mm) do {              \
    *addr = val;                                    \
    __asm volatile ("":::"memory");                 \
  } while (0)

#define sync_swap(addr, val, mm) __sync_lock_test_and_set (addr, val)

#define sync_cas(addr, from, to, onsuccess, onfailure)  \
  __sync_bool_compare_and_swap(addr, from, to)

#define sync_cas_val(addr, from, to, onsuccess, onfailure)  \
  __sync_val_compare_and_swap(addr, from, to)

#define sync_fadd(addr, val, mm) __sync_fetch_and_add(addr, val)

#define sync_addf(addr, val, mm) __sync_add_and_fetch(addr, val)

#define sync_fence(mm) __sync_synchronize()

#define SYNC_ATOMIC(decl) volatile decl

/* ../generic.h implements all of the strongly-typed versions in
 * terms of the above generic versions.
 */
#include "../generic.h"

#endif /* HPX_SYNC_GCC_SYNC_H_ */
