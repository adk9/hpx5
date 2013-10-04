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
#ifndef LIBHPX_SYNC_BUILTIN_COMMON_H_
#define LIBHPX_SYNC_BUILTIN_COMMON_H_

#define hpx_sync_load_i8(addr, mm) hpx_sync_load(addr, mm)
#define hpx_sync_load_i16(addr, mm) hpx_sync_load(addr, mm)
#define hpx_sync_load_i32(addr, mm) hpx_sync_load(addr, mm)
#define hpx_sync_load_i64(addr, mm) hpx_sync_load(addr, mm)
#define hpx_sync_load_i128(addr, mm) hpx_sync_load(addr, mm)
#define hpx_sync_load_ip(addr, mm) hpx_sync_load(addr, mm)
#define hpx_sync_load_ui8(addr, mm) hpx_sync_load(addr, mm)
#define hpx_sync_load_ui16(addr, mm) hpx_sync_load(addr, mm)
#define hpx_sync_load_ui32(addr, mm) hpx_sync_load(addr, mm)
#define hpx_sync_load_ui64(addr, mm) hpx_sync_load(addr, mm)
#define hpx_sync_load_ui128(addr, mm) hpx_sync_load(addr, mm)
#define hpx_sync_load_uip(addr, mm) hpx_sync_load(addr, mm)
#define hpx_sync_load_f(addr, mm) hpx_sync_load(addr, mm)
#define hpx_sync_load_d(addr, mm) hpx_sync_load(addr, mm)
#define hpx_sync_load_p(addr, mm) hpx_sync_load(addr, mm)

#define hpx_sync_store_i8(addr, val, mm) hpx_sync_store(addr, val, mm)
#define hpx_sync_store_i16(addr, val, mm) hpx_sync_store(addr, val, mm)
#define hpx_sync_store_i32(addr, val, mm) hpx_sync_store(addr, val, mm)
#define hpx_sync_store_i64(addr, val, mm) hpx_sync_store(addr, val, mm)
#define hpx_sync_store_i128(addr, val, mm) hpx_sync_store(addr, val, mm)
#define hpx_sync_store_ip(addr, val, mm) hpx_sync_store(addr, val, mm)
#define hpx_sync_store_ui8(addr, val, mm) hpx_sync_store(addr, val, mm)
#define hpx_sync_store_ui16(addr, val, mm) hpx_sync_store(addr, val, mm)
#define hpx_sync_store_ui32(addr, val, mm) hpx_sync_store(addr, val, mm)
#define hpx_sync_store_ui64(addr, val, mm) hpx_sync_store(addr, val, mm)
#define hpx_sync_store_ui128(addr, val, mm) hpx_sync_store(addr, val, mm)
#define hpx_sync_store_uip(addr, val, mm) hpx_sync_store(addr, val, mm)
#define hpx_sync_store_f(addr, val, mm) hpx_sync_store(addr, val, mm)
#define hpx_sync_store_d(addr, val, mm) hpx_sync_store(addr, val, mm)
#define hpx_sync_store_p(addr, val, mm) hpx_sync_store(addr, val, mm)

#define hpx_sync_swap_i8(addr, val, mm) hpx_sync_swap(addr, val, mm)
#define hpx_sync_swap_i16(addr, val, mm) hpx_sync_swap(addr, val, mm)
#define hpx_sync_swap_i32(addr, val, mm) hpx_sync_swap(addr, val, mm)
#define hpx_sync_swap_i64(addr, val, mm) hpx_sync_swap(addr, val, mm)
#define hpx_sync_swap_i128(addr, val, mm) hpx_sync_swap(addr, val, mm)
#define hpx_sync_swap_ip(addr, val, mm) hpx_sync_swap(addr, val, mm)
#define hpx_sync_swap_ui8(addr, val, mm) hpx_sync_swap(addr, val, mm)
#define hpx_sync_swap_ui16(addr, val, mm) hpx_sync_swap(addr, val, mm)
#define hpx_sync_swap_ui32(addr, val, mm) hpx_sync_swap(addr, val, mm)
#define hpx_sync_swap_ui64(addr, val, mm) hpx_sync_swap(addr, val, mm)
#define hpx_sync_swap_ui128(addr, val, mm) hpx_sync_swap(addr, val, mm)
#define hpx_sync_swap_uip(addr, val, mm) hpx_sync_swap(addr, val, mm)
#define hpx_sync_swap_f(addr, val, mm) hpx_sync_swap(addr, val, mm)
#define hpx_sync_swap_d(addr, val, mm) hpx_sync_swap(addr, val, mm)
#define hpx_sync_swap_p(addr, val,mm) hpx_sync_swap(addr, val, mm)

#define hpx_sync_cas_i8(addr, from, to, onsuccess, onfailure) hpx_sync_cas(addr, from, to, onsuccess, onfailure)
#define hpx_sync_cas_i16(addr, from, to, onsuccess, onfailure) hpx_sync_cas(addr, from, to, onsuccess, onfailure)
#define hpx_sync_cas_i32(addr, from, to, onsuccess, onfailure) hpx_sync_cas(addr, from, to, onsuccess, onfailure)
#define hpx_sync_cas_i64(addr, from, to, onsuccess, onfailure) hpx_sync_cas(addr, from, to, onsuccess, onfailure)
#define hpx_sync_cas_i128(addr, from, to, onsuccess, onfailure) hpx_sync_cas(addr, from, to, onsuccess, onfailure)
#define hpx_sync_cas_ip(addr, from, to, onsuccess, onfailure) hpx_sync_cas(addr, from, to, onsuccess, onfailure)
#define hpx_sync_cas_ui8(addr, from, to, onsuccess, onfailure) hpx_sync_cas(addr, from, to, onsuccess, onfailure)
#define hpx_sync_cas_ui16(addr, from, to, onsuccess, onfailure) hpx_sync_cas(addr, from, to, onsuccess, onfailure)
#define hpx_sync_cas_ui32(addr, from, to, onsuccess, onfailure) hpx_sync_cas(addr, from, to, onsuccess, onfailure)
#define hpx_sync_cas_ui64(addr, from, to, onsuccess, onfailure) hpx_sync_cas(addr, from, to, onsuccess, onfailure)
#define hpx_sync_cas_ui128(addr, from, to, onsuccess, onfailure) hpx_sync_cas(addr, from, to, onsuccess, onfailure)
#define hpx_sync_cas_uip(addr, from, to, onsuccess, onfailure) hpx_sync_cas(addr, from, to, onsuccess, onfailure)
#define hpx_sync_cas_f(addr, from, to, onsuccess, onfailure) hpx_sync_cas(addr, from, to, onsuccess, onfailure)
#define hpx_sync_cas_d(addr, from, to, onsuccess, onfailure) hpx_sync_cas(addr, from, to, onsuccess, onfailure)
#define hpx_sync_cas_p(addr, from, to, onsuccess, onfailure) hpx_sync_cas(addr, from, to, onsuccess, onfailure)

#define hpx_sync_cas_val_i8(addr, from, to, onsuccess, onfailure) hpx_sync_cas_val(addr, from, to, onsuccess, onfailure)
#define hpx_sync_cas_val_i16(addr, from, to, onsuccess, onfailure) hpx_sync_cas_val(addr, from, to, onsuccess, onfailure)
#define hpx_sync_cas_val_i32(addr, from, to, onsuccess, onfailure) hpx_sync_cas_val(addr, from, to, onsuccess, onfailure)
#define hpx_sync_cas_val_i64(addr, from, to, onsuccess, onfailure) hpx_sync_cas_val(addr, from, to, onsuccess, onfailure)
#define hpx_sync_cas_val_i128(addr, from, to, onsuccess, onfailure) hpx_sync_cas_val(addr, from, to, onsuccess, onfailure)
#define hpx_sync_cas_val_ip(addr, from, to, onsuccess, onfailure) hpx_sync_cas_val(addr, from, to, onsuccess, onfailure)
#define hpx_sync_cas_val_ui8(addr, from, to, onsuccess, onfailure) hpx_sync_cas_val(addr, from, to, onsuccess, onfailure)
#define hpx_sync_cas_val_ui16(addr, from, to, onsuccess, onfailure) hpx_sync_cas_val(addr, from, to, onsuccess, onfailure)
#define hpx_sync_cas_val_ui32(addr, from, to, onsuccess, onfailure) hpx_sync_cas_val(addr, from, to, onsuccess, onfailure)
#define hpx_sync_cas_val_ui64(addr, from, to, onsuccess, onfailure) hpx_sync_cas_val(addr, from, to, onsuccess, onfailure)
#define hpx_sync_cas_val_ui128(addr, from, to, onsuccess, onfailure) hpx_sync_cas_val(addr, from, to, onsuccess, onfailure)
#define hpx_sync_cas_val_uip(addr, from, to, onsuccess, onfailure) hpx_sync_cas_val(addr, from, to, onsuccess, onfailure)
#define hpx_sync_cas_val_f(addr, from, to, onsuccess, onfailure) hpx_sync_cas_val(addr, from, to, onsuccess, onfailure)
#define hpx_sync_cas_val_d(addr, from, to, onsuccess, onfailure) hpx_sync_cas_val(addr, from, to, onsuccess, onfailure)
#define hpx_sync_cas_val_p(addr, from, to, onsuccess, onfailure) hpx_sync_cas_val(addr, from, to, onsuccess, onfailure)

#define hpx_sync_fadd_i8(addr, val, mm) hpx_sync_fadd(addr, val, mm)
#define hpx_sync_fadd_i16(addr, val, mm) hpx_sync_fadd(addr, val, mm)
#define hpx_sync_fadd_i32(addr, val, mm) hpx_sync_fadd(addr, val, mm)
#define hpx_sync_fadd_i64(addr, val, mm) hpx_sync_fadd(addr, val, mm)
#define hpx_sync_fadd_i128(addr, val, mm) hpx_sync_fadd(addr, val, mm)
#define hpx_sync_fadd_ui8(addr, val, mm) hpx_sync_fadd(addr, val, mm)
#define hpx_sync_fadd_ui16(addr, val, mm) hpx_sync_fadd(addr, val, mm)
#define hpx_sync_fadd_ui32(addr, val, mm) hpx_sync_fadd(addr, val, mm)
#define hpx_sync_fadd_ui64(addr, val, mm) hpx_sync_fadd(addr, val, mm)
#define hpx_sync_fadd_ui128(addr, val, mm) hpx_sync_fadd(addr, val, mm)

#endif /* LIBHPX_SYNC_BUILTIN_COMMON_H_ */
