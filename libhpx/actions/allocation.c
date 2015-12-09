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

#include <string.h>
#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libhpx/padding.h>
#include <libhpx/parcel.h>
#include "table.h"

hpx_parcel_t *action_pack_args(hpx_parcel_t *p, int nargs, va_list *vargs) {
  dbg_assert(p);

  if (!nargs) {
    return p;
  }

  if (p->action == HPX_ACTION_NULL) {
    dbg_error("parcel must have an action to serialize arguments.\n");
  }

  const action_table_t *actions = here->actions;
  CHECK_BOUND(actions, p->action);
  const action_entry_t   *entry = &actions->entries[p->action];
  const ffi_cif            *cif = entry->cif;
  const char               *key = entry->key;
  uint32_t                 attr = entry->attr;

  bool marshalled = attr & HPX_MARSHALLED;
  bool     pinned = attr & HPX_PINNED;
  bool   vectored = attr & HPX_VECTORED;

  if (marshalled) {
    if (vectored) {
      nargs >>= 1;
      void *buf = hpx_parcel_get_data(p);
      *(int*)buf = nargs;
      size_t *sizes = (size_t*)((char*)buf + sizeof(int));
      void *args = (char*)sizes + (sizeof(size_t) * nargs);

      size_t n = ALIGN(args-buf, 8);
      for (int i = 0; i < nargs; ++i) {
        void *arg = va_arg(*vargs, void*);
        size_t size = va_arg(*vargs, int);
        sizes[i] = size;
        memcpy(args+n, arg, size);
        n += size + ALIGN(size, 8);
      }
    }
    return p;
  }

  // pinned actions have an implicit pointer, so update the caller's value
  if (pinned) {
    nargs++;
  }

  // if it's a 0-adic action, ignore the arguments
  if (cif->nargs == pinned) {
    return p;
  }

  if (nargs != cif->nargs) {
    dbg_error("%s requires %d arguments (%d given).\n", key, cif->nargs, nargs);
  }

  dbg_assert(ffi_raw_size((void*)cif) > 0);

  // copy the vaargs (pointers) to my stack array, which is what ffi wants
  void *argps[nargs];
  int i = 0;

  // special case pinned actions
  if (pinned) {
    argps[i++] = &argps[0];
  }

  // copy the individual vaargs
  while (i < nargs) {
    argps[i++] = va_arg(*vargs, void*);
  }

  // use ffi to copy them to the buffer
  ffi_raw *to = hpx_parcel_get_data(p);
  ffi_ptrarray_to_raw((void*)cif, argps, to);
  return p;
}


static hpx_parcel_t *_parcel_acquire(hpx_action_t id, int n, va_list *args) {
  dbg_assert(!n || args);

  if (!n) {
    return hpx_parcel_acquire(NULL, 0);
  }

  const action_table_t *actions = here->actions;
  CHECK_BOUND(actions, id);
  const action_entry_t *entry = &actions->entries[id];

  // Typed actions have fixed size stored in the ffi cif, and can ignore the
  // varargs.
  if (!entry_is_marshalled(entry)) {
    size_t bytes = ffi_raw_size(entry->cif);
    return hpx_parcel_acquire(NULL, bytes);
  }

  dbg_check(n & 1, "Untyped actions require even arg count: %d\n", n);

  // Marshalled actions pass the size (and data) in the varargs. We're doing
  // part of the argument packing here so we're not going to reuse the varargs.
  if (!entry_is_vectored(entry)) {
      void *data = va_arg(*args, void*);
      size_t bytes = va_arg(*args, int);
      return hpx_parcel_acquire(data, bytes);
  }

  // For vectored arguments we want to read through the argument pairs and
  // accumuluate the total number of bytes that we'll need to allocate.  We need
  // `int` bytes for the number of tuples, then `n/2` `size_t` bytes for the
  // size array, then padding to align the first buffer, and then 8-byte aligned
  // sizes for each buffer.
  int ntuples = n >> 1;
  size_t bytes = sizeof(int) + ntuples * sizeof(size_t);

  // The client will need to iterate the va_args again, so we make a copy, and
  // position its starting location on the first size. Then we just go through
  // the list, checking every other list element for the next size.
  va_list temp;
  va_copy(temp, *args);
  va_arg(temp, void*);
  for (int i = 0, e = ntuples; i < e; ++i, va_arg(temp, void*)) {
    bytes += ALIGN(n, 8);
    bytes += va_arg(temp, int);
  }
  va_end(temp);
  return hpx_parcel_acquire(NULL, bytes);
}

hpx_parcel_t *action_create_parcel_va(hpx_addr_t addr, hpx_action_t action,
                                      hpx_addr_t c_addr, hpx_action_t c_action,
                                      int nargs, va_list *args) {
  dbg_assert(addr);
  dbg_assert(action);
  hpx_parcel_t *p = _parcel_acquire(action, nargs, args);
  p->target = addr;
  p->action = action;
  p->c_target = c_addr;
  p->c_action = c_action;
  return action_pack_args(p, nargs, args);
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
