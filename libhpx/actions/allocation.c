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

#include <inttypes.h>
#include <string.h>
#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libhpx/padding.h>
#include <libhpx/parcel.h>
#include "table.h"

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

static void _pack_marshalled(const void *obj, void *b, int n, va_list *args) {
}

static void _pack_vectored(const void *obj, void *b, int n, va_list *vargs) {
  n >>= 1;
  *(int*)b = n;
  size_t *sizes = (size_t*)((char*)b + sizeof(int));
  void *args = (char*)sizes + (sizeof(size_t) * n);

  size_t bytes = ALIGN(args-b, 8);
  for (int i = 0; i < n; ++i) {
    void *arg = va_arg(*vargs, void*);
    size_t size = va_arg(*vargs, int);
    sizes[i] = size;
    memcpy(args+bytes, arg, size);
    bytes += size + ALIGN(size, 8);
  }
}

static hpx_parcel_t *_new_marshalled(const void *obj, hpx_addr_t addr,
                                     hpx_addr_t c_addr, hpx_action_t c_action,
                                     int n, va_list *args) {
  dbg_assert_str(!n || args);
  dbg_assert(!n || n == 2);

  const action_entry_t *entry = obj;
  hpx_action_t action = *entry->id;
  hpx_pid_t pid = hpx_thread_current_pid();
  void *data = (n) ? va_arg(*args, void*) : NULL;
  int bytes = (n) ? va_arg(*args, int) : 0;
  return parcel_new(addr, action, c_addr, c_action, pid, data, bytes);
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

static hpx_parcel_t *_new_vectored(const void *obj, hpx_addr_t addr,
                                   hpx_addr_t c_addr, hpx_action_t c_action,
                                   int n, va_list *args) {
  dbg_assert(n && args);
  dbg_assert_str(!(n & 1), "Vectored actions require even arg count: %d\n", n);

  const action_entry_t *entry = obj;
  hpx_action_t id = *entry->id;
  hpx_pid_t pid = hpx_thread_current_pid();

  // For vectored arguments we want to read through the argument pairs and
  // accumuluate the total number of bytes that we'll need to allocate.  We need
  // `int` bytes for the number of tuples, then `n/2` `size_t` bytes for the
  // size array, then padding to align the first buffer, and then 8-byte aligned
  // sizes for each buffer.
  int ntuples = n >> 1;
  size_t bytes = sizeof(int) + ntuples * sizeof(size_t);

  // We will need to iterate the va_args again, so we make a copy, and position
  // its starting location on the first size. Then we just go through the list,
  // checking every other list element for the next size.
  va_list temp;
  va_copy(temp, *args);
  va_arg(temp, void*);
  for (int i = 0, e = ntuples; i < e; ++i, va_arg(temp, void*)) {
    bytes += ALIGN(bytes, 8);
    bytes += va_arg(temp, int);
  }
  va_end(temp);

  hpx_parcel_t *p = parcel_new(addr, id, c_addr, c_action, pid, NULL, bytes);
  void *buffer = hpx_parcel_get_data(p);
  _pack_vectored(obj, buffer, n, args);
  return p;
}

void entry_init_new_parcel(action_entry_t *entry) {
  uint32_t attr = entry->attr & (HPX_PINNED | HPX_MARSHALLED | HPX_VECTORED);
  switch (attr) {
   case (HPX_ATTR_NONE):
    dbg_assert(entry->cif);
    entry->new_parcel = (entry->cif->nargs) ? _new_ffi_n : _new_ffi_0;
    return;
   case (HPX_PINNED):
    dbg_assert(entry->cif);
    entry->new_parcel = (entry->cif->nargs) ? _new_pinned_ffi_n : _new_ffi_0;
    return;
   case (HPX_MARSHALLED):
   case (HPX_PINNED | HPX_MARSHALLED):
    entry->new_parcel = _new_marshalled;
    return;
   case (HPX_MARSHALLED | HPX_VECTORED):
   case (HPX_PINNED | HPX_MARSHALLED | HPX_VECTORED):
    entry->new_parcel = _new_vectored;
    return;
  }
  dbg_error("Could not find a new parcel handler for attr %" PRIu32 "\n",
            entry->attr);
}

void entry_init_pack_buffer(action_entry_t *entry) {
  uint32_t attr = entry->attr & (HPX_PINNED | HPX_MARSHALLED | HPX_VECTORED);
  switch (attr) {
   case (HPX_ATTR_NONE):
    dbg_assert(entry->cif);
    entry->pack_buffer = (entry->cif->nargs) ? _pack_ffi_n : _pack_ffi_0;
    return;
   case (HPX_PINNED):
    dbg_assert(entry->cif);
    entry->pack_buffer = (entry->cif->nargs) ? _pack_pinned_ffi_n : _pack_ffi_0;
    return;
   case (HPX_MARSHALLED):
   case (HPX_PINNED | HPX_MARSHALLED):
    entry->pack_buffer = _pack_marshalled;
    return;
   case (HPX_MARSHALLED | HPX_VECTORED):
   case (HPX_PINNED | HPX_MARSHALLED | HPX_VECTORED):
    entry->pack_buffer = _pack_vectored;
    return;
  }
  dbg_error("Could not find a new pack buffer handler for attr %" PRIu32 "\n",
            entry->attr);
}

hpx_parcel_t *action_pack_args(hpx_parcel_t *p, int n, va_list *args) {
  CHECK_BOUND(&actions, p->action);
  const action_entry_t *entry = &actions.entries[p->action];
  void *buffer = hpx_parcel_get_data(p);
  entry->pack_buffer(entry, buffer, n, args);
  return p;
}

hpx_parcel_t *action_create_parcel_va(hpx_addr_t addr, hpx_action_t action,
                                      hpx_addr_t c_addr, hpx_action_t c_action,
                                      int nargs, va_list *args) {
  CHECK_BOUND(&actions, action);
  const action_entry_t *entry = &actions.entries[action];
  return entry->new_parcel(entry, addr, c_addr, c_action, nargs, args);
}

hpx_parcel_t *action_create_parcel(hpx_addr_t addr, hpx_action_t action,
                                   hpx_addr_t c_addr, hpx_action_t c_action,
                                   int nargs, ...) {
  va_list args;
  va_start(args, nargs);
  hpx_parcel_t *p = action_create_parcel_va(addr, action, c_addr, c_action,
                                            nargs, &args);
  va_end(args);
  return p;
}
