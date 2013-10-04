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
#ifndef LIBHPX_SYNC_SYNC_H_
#define LIBHPX_SYNC_SYNC_H_

/*
  ====================================================================
  This file defines a basic, low-level interface to the
  synchronization primitives used in libhpx. This accounts for
  architecture and compiler-specific differences. It is designed based
  on gcc's builtin atomic interface.

  Wherever possible, we prefer compiler builtins to library-based, or
  inline asm-based implementations of this interface. These
  implementations are done in platform-specific headers, which we
  selectively include based on ifdefs.

  If that fails, this file provides a library-based interface to the
  synchronization that we desire.
  ====================================================================
*/

#if defined(__ATOMIC_ACQUIRE)
#include "sync/gcc_builtin_atomic.h"
#elif defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_1)
#include "sync/gcc_builtin_sync.h"
#elif defined(_CRAYC)
#include "sync/craycc_builtin_sync.h"
#else

#define HPX_SYNC_RELAXED 0
#define HPX_SYNC_CONSUME 1
#define HPX_SYNC_ACQUIRE 2
#define HPX_SYNC_RELEASE 3
#define HPX_SYNC_ACQ_REL 4
#define HPX_SYNC_SEQ_CST 5

#ifdef __GNUC__ && __i686__
#define FASTCALL __attribute__((fastcall));
#else
#define FASTCALL
#endif

/*
  ====================================================================
  No builtin support for atomics, use a library-based implementation.
  ====================================================================
*/

#include <stdbool.h>
#include <stdint.h>

/*
  ====================================================================
  Load a value atomically. Where relaxed consistency is available,
  this is performed using "acquire" semantics.
  ====================================================================
*/

int8_t      hpx_sync_load_i8(int8_t*, int) FASTCALL;
int16_t     hpx_sync_load_i16(int16_t*, int) FASTCALL;
int32_t     hpx_sync_load_i32(int32_t*, int) FASTCALL;
int64_t     hpx_sync_load_i64(int64_t*, int) FASTCALL;
__int128_t  hpx_sync_load_i128(__int128_t*, int) FASTCALL;
intptr_t    hpx_sync_load_ip(intptr_t*, int) FASTCALL;
uint8_t     hpx_sync_load_ui8(uint8_t*, int) FASTCALL;
uint16_t    hpx_sync_load_ui16(uint16_t*, int) FASTCALL;
uint32_t    hpx_sync_load_ui32(uint32_t*, int) FASTCALL;
uint64_t    hpx_sync_load_ui64(uint64_t*, int) FASTCALL;
__uint128_t hpx_sync_load_ui128(__uint128_t*, int) FASTCALL;
uintptr_t   hpx_sync_load_uip(uintptr_t*, int) FASTCALL;
float       hpx_sync_load_f(float*, int) FASTCALL;
double      hpx_sync_load_d(double*, int) FASTCALL;
void*       hpx_sync_load_p(void**, int) FASTCALL;

#if __STDC_VERSION__ == 201112L
#define hpx_sync_load(addr, mm)                     \
    _Generic((addr),                                \
        int8_t*      :hpx_sync_load_i8,             \
        int16_t*     :hpx_sync_load_i16,            \
        int32_t*     :hpx_sync_load_i32,            \
        int64_t*     :hpx_sync_load_i64,            \
        __int128_t*  :hpx_sync_load_i128,           \
        intptr_t*    :hpx_sync_load_ip,             \
        uint8_t*     :hpx_sync_load_ui8,            \
        uint16_t*    :hpx_sync_load_ui16,           \
        uint32_t*    :hpx_sync_load_ui32,           \
        uint64_t*    :hpx_sync_load_ui64,           \
        __uint128_t* :hpx_sync_load_ui128,          \
        uintptr_t*   :hpx_sync_load_uip,            \
        float*       :hpx_sync_load_f,              \
        double*      :hpx_sync_load_d,              \
        void**       :hpx_sync_load_p)(addr, mm)
#else
#warning No support for generic synchronization for your platform.
#endif

/*
  ====================================================================
  Store a value atomically. Where relaxed consistency is available,
  this is performed using "release" semantics.
  ====================================================================
*/

void hpx_sync_store_i8(int8_t*, int8_t, int) FASTCALL;
void hpx_sync_store_i16(int16_t*, int16_t, int) FASTCALL;
void hpx_sync_store_i32(int32_t*, int32_t, int) FASTCALL;
void hpx_sync_store_i64(int64_t*, int64_t, int) FASTCALL;
void hpx_sync_store_i128(__int128_t*, __int128_t, int) FASTCALL;
void hpx_sync_store_ip(intptr_t*, intptr_t, int) FASTCALL;
void hpx_sync_store_ui8(uint8_t*, uint8_t, int) FASTCALL;
void hpx_sync_store_ui16(uint16_t*, uint16_t, int) FASTCALL;
void hpx_sync_store_ui32(uint32_t*, uint32_t, int) FASTCALL;
void hpx_sync_store_ui64(uint64_t*, uint64_t, int) FASTCALL;
void hpx_sync_store_ui128(__uint128_t*, __uint128_t, int) FASTCALL;
void hpx_sync_store_uip(uintptr_t*, uintptr_t, int) FASTCALL;
void hpx_sync_store_f(float*, float, int) FASTCALL;
void hpx_sync_store_d(double*, double, int) FASTCALL;
void hpx_sync_store_p(void**, void*, int) FASTCALL;

#if __STDC_VERSION__ == 201112L
#define hpx_sync_store(addr, val, mm)                   \
    _Generic((addr),                                    \
        int8_t*      :hpx_sync_store_i8,                \
        int16_t*     :hpx_sync_store_i16,               \
        int32_t*     :hpx_sync_store_i32,               \
        int64_t*     :hpx_sync_store_i64,               \
        __int128_t*  :hpx_sync_store_i128,              \
        intptr_t*    :hpx_sync_store_ip,                \
        uint8_t*     :hpx_sync_store_ui8,               \
        uint16_t*    :hpx_sync_store_ui16,              \
        uint32_t*    :hpx_sync_store_ui32,              \
        uint64_t*    :hpx_sync_store_ui64,              \
        __uint128_t* :hpx_sync_store_ui128,             \
        uintptr_t*   :hpx_sync_store_uip,               \
        float*       :hpx_sync_store_f,                 \
        double*      :hpx_sync_store_d,                 \
        void**       :hpx_sync_store_p)(addr, val, mm)
#else
#warning No support for generic synchronization for your platform.
#endif

/*
  ====================================================================
  Atomically swap a value with memory.
  ====================================================================
*/

int8_t      hpx_sync_swap_i8(int8_t*, int8_t, int) FASTCALL;
int16_t     hpx_sync_swap_i16(int16_t*, int16_t, int) FASTCALL;
int32_t     hpx_sync_swap_i32(int32_t*, int32_t, int) FASTCALL;
int64_t     hpx_sync_swap_i64(int64_t*, int64_t, int) FASTCALL;
__int128_t  hpx_sync_swap_i128(__int128_t*, __int128_t, int) FASTCALL;
intptr_t    hpx_sync_swap_ip(intptr_t*, intptr_t, int) FASTCALL;
uint8_t     hpx_sync_swap_ui8(uint8_t*, uint8_t, int) FASTCALL;
uint16_t    hpx_sync_swap_ui16(uint16_t*, uint16_t, int) FASTCALL;
uint32_t    hpx_sync_swap_ui32(uint32_t*, uint32_t, int) FASTCALL;
uint64_t    hpx_sync_swap_ui64(uint64_t*, uint64_t, int) FASTCALL;
__uint128_t hpx_sync_swap_ui128(__uint128_t*, __uint128_t, int) FASTCALL;
uintptr_t   hpx_sync_swap_uip(uintptr_t*, uintptr_t, int) FASTCALL;
float       hpx_sync_swap_f(float*, float, int) FASTCALL;
double      hpx_sync_swap_d(double*, double, int) FASTCALL;
void*       hpx_sync_swap_p(void**, void*, int) FASTCALL;

#if __STDC_VERSION__ == 201112L
#define hpx_sync_swap(addr, val, mm)                    \
    _Generic((addr),                                    \
        int8_t*      :hpx_sync_swap_i8,                 \
        int16_t*     :hpx_sync_swap_i16,                \
        int32_t*     :hpx_sync_swap_i32,                \
        int64_t*     :hpx_sync_swap_i64,                \
        __int128_t*  :hpx_sync_swap_i128,               \
        intptr_t*    :hpx_sync_swap_ip,                 \
        uint8_t*     :hpx_sync_swap_ui8,                \
        uint16_t*    :hpx_sync_swap_ui16,               \
        uint32_t*    :hpx_sync_swap_ui32,               \
        uint64_t*    :hpx_sync_swap_ui64,               \
        __uint128_t* :hpx_sync_swap_ui128,              \
        uintptr_t*   :hpx_sync_swap_uip,                \
        float*       :hpx_sync_swap_f,                  \
        double*      :hpx_sync_swap_d,                  \
        void**       :hpx_sync_swap_p)(addr, val, mm)
#else
#warning No support for generic synchronization for your platform.
#endif

/*
  ====================================================================
  Boolean compare-and-exchange.
  ====================================================================
*/

bool hpx_sync_cas_i8(int8_t*, int8_t, int8_t, int, int) FASTCALL;
bool hpx_sync_cas_i16(int16_t*, int16_t, int16_t, int, int) FASTCALL;
bool hpx_sync_cas_i32(int32_t*, int32_t, int32_t, int, int) FASTCALL;
bool hpx_sync_cas_i64(int64_t*, int64_t, int64_t, int, int) FASTCALL;
bool hpx_sync_cas_i128(__int128_t*, __int128_t, __int128_t, int, int) FASTCALL;
bool hpx_sync_cas_ip(intptr_t*, intptr_t, intptr_t, int, int) FASTCALL;
bool hpx_sync_cas_ui8(uint8_t*, uint8_t, uint8_t, int, int) FASTCALL;
bool hpx_sync_cas_ui16(uint16_t*, uint16_t, uint16_t, int, int) FASTCALL;
bool hpx_sync_cas_ui32(uint32_t*, uint32_t, uint32_t, int, int) FASTCALL;
bool hpx_sync_cas_ui64(uint64_t*, uint64_t, uint64_t, int, int) FASTCALL;
bool hpx_sync_cas_ui128(__uint128_t*, __uint128_t, __uint128_t, int, int) FASTCALL;
bool hpx_sync_cas_uip(uintptr_t*, uintptr_t, uintptr_t, int, int) FASTCALL;
bool hpx_sync_cas_f(float*, float, float, int, int) FASTCALL;
bool hpx_sync_cas_d(double*, double, double, int, int) FASTCALL;
bool hpx_sync_cas_p(void**, void*, void*, int, int) FASTCALL;

#if __STDC_VERSION__ == 201112L
#define hpx_sync_cas(addr, from, to, onsucces, onfailure)               \
    _Generic((addr),                                                    \
        int8_t*      :hpx_sync_cas_i8,                                  \
        int16_t*     :hpx_sync_cas_i16,                                 \
        int32_t*     :hpx_sync_cas_i32,                                 \
        int64_t*     :hpx_sync_cas_i64,                                 \
        __int128_t*  :hpx_sync_cas_i128,                                \
        intptr_t*    :hpx_sync_cas_ip,                                  \
        uint8_t*     :hpx_sync_cas_ui8,                                 \
        uint16_t*    :hpx_sync_cas_ui16,                                \
        uint32_t*    :hpx_sync_cas_ui32,                                \
        uint64_t*    :hpx_sync_cas_ui64,                                \
        __uint128_t* :hpx_sync_cas_ui128,                               \
        uintptr_t*   :hpx_sync_cas_uip,                                 \
        float*       :hpx_sync_cas_f,                                   \
        double*      :hpx_sync_cas_d,                                   \
        void**       :hpx_sync_cas_p)(addr, from, to, onsuccess, onfailure)
#else
#warning No support for generic synchronization for your platform.
#endif

/*
  ====================================================================
  Value-based compare and exchange, returns the actual value that was
  seen at the address.
  ====================================================================
*/

int8_t      hpx_sync_cas_val_i8(int8_t*, int8_t, int8_t, int, int) FASTCALL;
int16_t     hpx_sync_cas_val_i16(int16_t*, int16_t, int16_t, int, int) FASTCALL;
int32_t     hpx_sync_cas_val_i32(int32_t*, int32_t, int32_t, int, int) FASTCALL;
int64_t     hpx_sync_cas_val_i64(int64_t*, int64_t, int64_t, int, int) FASTCALL;
__int128_t  hpx_sync_cas_val_i128(__int128_t*, __int128_t, __int128_t, int, int) FASTCALL;
intptr_t    hpx_sync_cas_val_ip(intptr_t*, intptr_t, intptr_t, int, int) FASTCALL;
uint8_t     hpx_sync_cas_val_ui8(uint8_t*, uint8_t, uint8_t, int, int) FASTCALL;
uint16_t    hpx_sync_cas_val_ui16(uint16_t*, uint16_t, uint16_t, int, int) FASTCALL;
uint32_t    hpx_sync_cas_val_ui32(uint32_t*, uint32_t, uint32_t, int, int) FASTCALL;
uint64_t    hpx_sync_cas_val_ui64(uint64_t*, uint64_t, uint64_t, int, int) FASTCALL;
__uint128_t hpx_sync_cas_val_ui128(__uint128_t*, __uint128_t, __uint128_t, int, int) FASTCALL;
uintptr_t   hpx_sync_cas_val_uip(uintptr_t*, uintptr_t, uintptr_t, int, int) FASTCALL;
float       hpx_sync_cas_val_f(float*, float, float, int, int) FASTCALL;
double      hpx_sync_cas_val_d(double*, double, double, int, int) FASTCALL;
void*       hpx_sync_cas_val_p(void**, void*, void*, int, int) FASTCALL;

#if __STDC_VERSION__ == 201112L
#define hpx_sync_cas_val(addr, from, to, onsucces, onfailure)           \
    _Generic((addr),                                                    \
        int8_t*      :hpx_sync_cas_val_i8,                              \
        int16_t*     :hpx_sync_cas_val_i16,                             \
        int32_t*     :hpx_sync_cas_val_i32,                             \
        int64_t*     :hpx_sync_cas_val_i64,                             \
        __int128_t*  :hpx_sync_cas_val_i128,                            \
        intptr_t*    :hpx_sync_cas_val_ip,                              \
        uint8_t*     :hpx_sync_cas_val_ui8,                             \
        uint16_t*    :hpx_sync_cas_val_ui16,                            \
        uint32_t*    :hpx_sync_cas_val_ui32,                            \
        uint64_t*    :hpx_sync_cas_val_ui64,                            \
        __uint128_t* :hpx_sync_cas_val_ui128,                           \
        uintptr_t*   :hpx_sync_cas_val_uip,                             \
        float*       :hpx_sync_cas_val_f,                               \
        double*      :hpx_sync_cas_val_d,                               \
        void**       :hpx_sync_cas_val_p)(addr, from, to, onsuccess, onfailure)
#else
#warning No support for generic synchronization for your platform.
#endif

/*
  ====================================================================
  Atomic fetch-and-add for integer types.
  ====================================================================
*/

int8_t      hpx_sync_fadd_i8(int8_t*, int8_t, int) FASTCALL;
int16_t     hpx_sync_fadd_i16(int16_t*, int16_t, int) FASTCALL;
int32_t     hpx_sync_fadd_i32(int32_t*, int32_t, int) FASTCALL;
int64_t     hpx_sync_fadd_i64(int64_t*, int64_t, int) FASTCALL;
__int128_t  hpx_sync_fadd_i128(__int128_t*, __int128_t, int) FASTCALL;
uint8_t     hpx_sync_fadd_ui8(uint8_t*, int8_t, int) FASTCALL;
uint16_t    hpx_sync_fadd_ui16(uint16_t*, int16_t, int) FASTCALL;
uint32_t    hpx_sync_fadd_ui32(uint32_t*, int32_t, int) FASTCALL;
uint64_t    hpx_sync_fadd_ui64(uint64_t*, int64_t, int) FASTCALL;
__uint128_t hpx_sync_fadd_ui128(__uint128_t*, __int128_t, int) FASTCALL;

#if __STDC_VERSION__ == 201112L
#define hpx_sync_fadd(addr, val, mm)                        \
    _Generic((addr),                                        \
        int8_t*      :hpx_sync_fadd_i8,                     \
        int16_t*     :hpx_sync_fadd_i16,                    \
        int32_t*     :hpx_sync_fadd_i32,                    \
        int64_t*     :hpx_sync_fadd_i64,                    \
        __int128_t*  :hpx_sync_fadd_i128,                   \
        uint8_t*     :hpx_sync_fadd_ui8,                    \
        uint16_t*    :hpx_sync_fadd_ui16,                   \
        uint32_t*    :hpx_sync_fadd_ui32,                   \
        uint64_t*    :hpx_sync_fadd_ui64,                   \
        __uint128_t* :hpx_sync_fadd_ui128)(addr, val, mm)
#else
#warning No support for generic synchronization for your platform.
#endif

/*
  ====================================================================
  Implements a fence based on the memory model.
  ====================================================================
*/

void hpx_sync_fence(int);

#undef FASTCALL

#endif

#endif /* LIBHPX_SYNC_SYNC_H_ */
