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

#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libhpx/locality.h>
#include <libhpx/padding.h>
#include <libhpx/parcel.h>
#include "table.h"

int action_execute(hpx_parcel_t *p) {
  dbg_assert(p->target != HPX_NULL);
  dbg_assert_str(p->action != HPX_ACTION_INVALID, "registration error\n");
  dbg_assert_str(p->action < here->actions->n,
                 "action, %d, out of bounds [0,%u)\n",
                 p->action, here->actions->n);

  hpx_action_t              id = p->action;
  const action_table_t  *table = here->actions;
  CHECK_BOUND(table, id);
  const action_entry_t  *entry = &table->entries[id];
  hpx_action_handler_t handler = entry->handler;
  dbg_assert(handler);
  ffi_cif                 *cif = entry->cif;
  bool              marshalled = action_is_marshalled(table, id);
  bool                  pinned = action_is_pinned(table, id);
  bool                vectored = action_is_vectored(table, id);
  void                   *args = hpx_parcel_get_data(p);

  if (!pinned && marshalled) {
    if (likely(!vectored)) {
      return handler(args, p->size);
    }

    int nargs = *(int*)args;
    size_t *sizes = (size_t*)((char*)args + sizeof(int));
    void *argsp[nargs];
    void *vargs = (char*)sizes + (nargs * sizeof(size_t));
    argsp[0] = (char*)vargs + ALIGN(vargs-args, 8);

    for (int i = 0; i < nargs-1; ++i) {
      argsp[i+1] = (char*)argsp[i] + sizes[i] + ALIGN(sizes[i], 8);
    }
    return ((hpx_vectored_action_handler_t)handler)(nargs, argsp, sizes);
  }

  if (!pinned) {
    char ffiret[8];               // https://github.com/atgreen/libffi/issues/35
    int *ret = (int*)&ffiret[0];
    ffi_raw_call(cif, FFI_FN(handler), ret, args);
    return *ret;
  }

  void *target;
  if (!hpx_gas_try_pin(p->target, &target)) {
    log_action("pinned action resend.\n");
    return HPX_RESEND;
  }

  if (marshalled) {
    if (likely(!vectored)) {
      return ((hpx_pinned_action_handler_t)handler)(target, args, p->size);
    }

    int nargs = *(int*)args;
    size_t *sizes = (size_t*)((char*)args + sizeof(int));
    void *argsp[nargs];
    void *vargs = (char*)sizes + (nargs * sizeof(size_t));
    argsp[0] = (char*)vargs + ALIGN(vargs-args, 8);

    for (int i = 0; i < nargs-1; ++i) {
      argsp[i+1] = (char*)argsp[i] + sizes[i] + ALIGN(sizes[i], 8);
    }
    return ((hpx_pinned_vectored_action_handler_t)handler)(target, nargs,
                                                           &argsp, sizes);
  }

  void *avalue[cif->nargs];
  ffi_raw_to_ptrarray(cif, args, avalue);
  avalue[0] = &target;
  char ffiret[8];               // https://github.com/atgreen/libffi/issues/35
  int *ret = (int*)&ffiret[0];
  ffi_call(cif, FFI_FN(handler), ret, avalue);
  return *ret;
}
