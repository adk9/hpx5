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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <hpx/hpx.h>
#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libhpx/parcel.h>
#include "init.h"

static void _pack_ffi_0(const void *obj, void *b, int n, va_list *args) {
  // nothing to do
}

static void _pack_ffi_n(const void *obj, void *b, int n, va_list *args) {
  const action_entry_t *entry = obj;
  const ffi_cif          *cif = entry->cif;

  DEBUG_IF (n != cif->nargs) {
    const char *key = entry->key;
    dbg_error("%s requires %d arguments (%d given).\n", key, cif->nargs, n);
  }

  // copy the vaargs (pointers) to my stack array, which is what ffi
  // wants---this seems wasteful since va_args are likely *already* on my stack,
  // but that's not public information
  void *argps[n];
  for (int i = 0, e = n; i < e; ++i) {
    argps[i] = va_arg(*args, void*);
  }

  // use ffi to copy them to the buffer
  ffi_ptrarray_to_raw((void*)cif, argps, b);
}

static void _pack_pinned_ffi_n(const void *obj, void *b, int n, va_list *args) {
  const action_entry_t *entry = obj;
  const ffi_cif          *cif = entry->cif;

  DEBUG_IF (n + 1 != cif->nargs) {
    const char *key = entry->key;
    dbg_error("%s requires %d arguments (%d given).\n", key, cif->nargs, n + 1);
  }

  // Copy the vaargs (pointers) to my stack array, which is what ffi wants,
  // adding an extra "slot" for the pinned parameter.
  void *argps[++n];

  // special case pinned actions
  argps[0] = &argps[0];

  // copy the individual vaargs
  for (int i = 1, e = n; i < e; ++i) {
    argps[i] = va_arg(*args, void*);
  }

  // use ffi to copy them to the buffer
  ffi_ptrarray_to_raw((void*)cif, argps, b);
}

static hpx_parcel_t *_new_ffi_0(const void *obj, hpx_addr_t addr,
                                hpx_addr_t c_addr, hpx_action_t c_action,
                                int n, va_list *args) {
  const action_entry_t *entry = obj;
  hpx_action_t id = *entry->id;
  hpx_pid_t pid = hpx_thread_current_pid();
  return parcel_new(addr, id, c_addr, c_action, pid, NULL, 0);
}

static hpx_parcel_t *_new_ffi_n(const void *obj, hpx_addr_t addr,
                                hpx_addr_t c_addr, hpx_action_t c_action,
                                int n, va_list *args) {
  const action_entry_t *entry = obj;
  hpx_action_t id = *entry->id;
  hpx_pid_t pid = hpx_thread_current_pid();
  size_t bytes = ffi_raw_size(entry->cif);
  hpx_parcel_t *p = parcel_new(addr, id, c_addr, c_action, pid, NULL, bytes);
  void *buffer = hpx_parcel_get_data(p);
  _pack_ffi_n(obj, buffer, n, args);
  return p;
}

static hpx_parcel_t *_new_pinned_ffi_n(const void *obj, hpx_addr_t addr,
                                       hpx_addr_t c_addr, hpx_action_t c_action,
                                       int n, va_list *args) {
  const action_entry_t *entry = obj;
  hpx_action_t id = *entry->id;
  hpx_pid_t pid = hpx_thread_current_pid();
  size_t bytes = ffi_raw_size(entry->cif);
  hpx_parcel_t *p = parcel_new(addr, id, c_addr, c_action, pid, NULL, bytes);
  void *buffer = hpx_parcel_get_data(p);
  _pack_pinned_ffi_n(obj, buffer, n, args);
  return p;
}

static int _execute_ffi_n(const void *obj, hpx_parcel_t *p) {
  const action_entry_t *entry = obj;
  char ffiret[8];               // https://github.com/atgreen/libffi/issues/35
  int *ret = (int*)&ffiret[0];
  void *args = hpx_parcel_get_data(p);
  ffi_raw_call(entry->cif, entry->handler, ret, args);
  return *ret;
}

static int _execute_pinned_ffi_n(const void *obj, hpx_parcel_t *p) {
  void *target;
  if (!hpx_gas_try_pin(p->target, &target)) {
    log_action("pinned action resend.\n");
    return HPX_RESEND;
  }

  const action_entry_t *entry = obj;
  ffi_cif *cif = entry->cif;
  void *args = hpx_parcel_get_data(p);
  void *avalue[cif->nargs];
  ffi_raw_to_ptrarray(cif, args, avalue);
  avalue[0] = &target;
  char ffiret[8];               // https://github.com/atgreen/libffi/issues/35
  int *ret = (int*)&ffiret[0];
  ffi_call(cif, entry->handler, ret, avalue);
  return *ret;
}

void entry_init_ffi(action_entry_t *entry) {
  dbg_assert(entry->cif);
  if (!entry->cif->nargs) {
    entry->new_parcel = _new_ffi_0;
    entry->pack_buffer = _pack_ffi_0;
  }
  else if (entry_is_pinned(entry)) {
    entry->new_parcel = _new_pinned_ffi_n;
    entry->pack_buffer = _pack_pinned_ffi_n;
  }
  else {
    entry->new_parcel = _new_ffi_n;
    entry->pack_buffer = _pack_ffi_n;
  }

  if (entry_is_pinned(entry)) {
    entry->execute_parcel = _execute_pinned_ffi_n;
  }
  else {
    entry->execute_parcel = _execute_ffi_n;
  }
}
