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
#ifndef HPX_SYNC_CRAYCC_SYNC_H_
#define HPX_SYNC_CRAYCC_SYNC_H_

/*
  ====================================================================
  Defines our synchronization interface in terms of the craycc
  __sync builtins defined in "Cray C and C++ Reference Manual."

  http://docs.cray.com/books/S-2179-81/S-2179-81.pdf
  ====================================================================
*/

#include <intrinsics.h>

/* no memory model */
#define SYNC_RELAXED 0
#define SYNC_CONSUME 0
#define SYNC_ACQ_REL 0
#define SYNC_SEQ_CST 0
#define SYNC_ACQUIRE 0
#define SYNC_RELEASE 0


#define sync_load(val, addr, mm) do {           \
    __builtin_ia32_lfence();                    \
    val = *addr;                                \
  } while (0)


#define sync_store(addr, val, mm) do {          \
    *addr = val;                                \
    __builtin_ia32_sfence();                    \
  } while (0)

#define sync_swap(addr, val, mm)                \
  __sync_lock_test_and_set(addr, val)

#define sync_cas(addr, from, to, onsuccess, onfailure) \
  (__sync_val_compare_and_swap(addr, from, to) == from)

#define sync_cas_val(addr, from, to, onsuccess, onfailure)  \
  __sync_val_compare_and_swap(addr, from, to)

#define sync_fadd(addr, val, mm) __sync_fetch_and_add(addr, val)

#define sync_addf(addr, val, mm) __sync_add_and_fetch(addr, val)

#define sync_fence(mm) __builtin_ia32_mfence()

#define SYNC_ATOMIC(decl) volatile decl

/* ../generic.h implements all of the strongly-typed versions in
 * terms of the above generic versions.
 */
#include "../generic.h"

#endif /* HPX_SYNC_CRAYCC_SYNC_H_ */
