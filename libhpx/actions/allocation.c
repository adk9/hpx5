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


static hpx_parcel_t *_action_parcel_acquire(hpx_action_t action, int nargs,
                                            va_list *vargs) {
  if (!vargs) {
    return hpx_parcel_acquire(NULL, 0);
  }

  const action_table_t *actions = here->actions;
  bool marshalled = action_is_marshalled(actions, action);
  bool   vectored = action_is_vectored(actions, action);

  if (marshalled) {
    if (likely(!vectored)) {
      void *data = va_arg(*vargs, void*);
      size_t n = va_arg(*vargs, int);
      return hpx_parcel_acquire(data, n);
    } else {
      va_list temp;
      va_copy(temp, *vargs);

      int e = nargs % 2;
      dbg_assert_str(!e, "invalid number of vectored arguments %d\n", nargs);

      size_t n = 8;
      for (int i = 0; i < nargs; i += 2) {
        void *data = va_arg(temp, void*);
        size_t size = va_arg(temp, int);
        n += (sizeof(size_t) + size + ALIGN(size, 8));
        (void)data;
      }
      va_end(temp);
      return hpx_parcel_acquire(NULL, n + sizeof(int));
    }
  } else {
    CHECK_BOUND(actions, action);
    const action_entry_t *entry = &actions->entries[action];
    const ffi_cif *cif = entry->cif;
    dbg_assert(cif);
    size_t n = ffi_raw_size((void*)cif);
    return hpx_parcel_acquire(NULL, n);
  }
}

hpx_parcel_t *action_create_parcel_va(hpx_addr_t addr, hpx_action_t action,
                                      hpx_addr_t c_addr, hpx_action_t c_action,
                                      int nargs, va_list *args) {
  dbg_assert(addr);
  dbg_assert(action);
  hpx_parcel_t *p = _action_parcel_acquire(action, nargs, args);
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
