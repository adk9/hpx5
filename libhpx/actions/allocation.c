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

static hpx_parcel_t *_new_ffi(const void *obj, hpx_addr_t addr,
                              hpx_addr_t c_addr, hpx_action_t c_action,
                              int n, va_list *args) {
  const action_entry_t *entry = obj;
  hpx_action_t id = *entry->id;
  hpx_pid_t pid = hpx_thread_current_pid();
  size_t bytes = ffi_raw_size(entry->cif);
  hpx_parcel_t *p = parcel_new(addr, id, c_addr, c_action, pid, NULL, bytes);
  return action_pack_args(p, n, args);
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
  return action_pack_args(p, n, args);
}

void entry_init_new_parcel(action_entry_t *entry) {
  uint32_t attr = entry->attr & (HPX_MARSHALLED | HPX_VECTORED);
  switch (attr) {
   case (HPX_ATTR_NONE):
    entry->new_parcel = _new_ffi;
    return;
   case (HPX_MARSHALLED):
    entry->new_parcel = _new_marshalled;
    return;
   case (HPX_MARSHALLED | HPX_VECTORED):
    entry->new_parcel = _new_vectored;
    return;
  }
  dbg_error("Could not find a new parcel handler for attr %" PRIu32 "\n",
            entry->attr);
}

hpx_parcel_t *action_create_parcel_va(hpx_addr_t addr, hpx_action_t action,
                                      hpx_addr_t c_addr, hpx_action_t c_action,
                                      int nargs, va_list *args) {
  const action_table_t *table = here->actions;
  CHECK_BOUND(table, action);
  const action_entry_t *entry = &table->entries[action];
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
