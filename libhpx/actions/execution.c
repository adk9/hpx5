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
#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libhpx/locality.h>
#include <libhpx/padding.h>
#include <libhpx/parcel.h>
#include "table.h"

static int _execute_marshalled(const void *obj, hpx_parcel_t *p) {
  const action_entry_t *entry = obj;
  hpx_action_handler_t handler = (hpx_action_handler_t)entry->handler;
  void *args = hpx_parcel_get_data(p);
  return handler(args, p->size);
}

static int _execute_pinned_marshalled(const void *obj, hpx_parcel_t *p) {
  void *target;
  if (!hpx_gas_try_pin(p->target, &target)) {
    log_action("pinned action resend.\n");
    return HPX_RESEND;
  }

  const action_entry_t *entry = obj;
  hpx_pinned_action_handler_t handler =
      (hpx_pinned_action_handler_t)entry->handler;
  void *args = hpx_parcel_get_data(p);
  return handler(target, args, p->size);
}

static int _execute_ffi(const void *obj, hpx_parcel_t *p) {
  const action_entry_t *entry = obj;
  char ffiret[8];               // https://github.com/atgreen/libffi/issues/35
  int *ret = (int*)&ffiret[0];
  void *args = hpx_parcel_get_data(p);
  ffi_raw_call(entry->cif, entry->handler, ret, args);
  return *ret;
}

static int _execute_pinned_ffi(const void *obj, hpx_parcel_t *p) {
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

static int _execute_vectored(const void *obj, hpx_parcel_t *p) {
  const action_entry_t *entry = obj;
  void *args = hpx_parcel_get_data(p);
  int nargs = *(int*)args;
  size_t *sizes = (size_t*)((char*)args + sizeof(int));
  void *argsp[nargs];
  void *vargs = (char*)sizes + (nargs * sizeof(size_t));
  argsp[0] = (char*)vargs + ALIGN(vargs-args, 8);

  for (int i = 0, e = nargs - 1; i < e; ++i) {
    argsp[i + 1] = (char*)argsp[i] + sizes[i] + ALIGN(sizes[i], 8);
  }

  hpx_vectored_action_handler_t handler =
      (hpx_vectored_action_handler_t )entry->handler;
  return handler(nargs, argsp, sizes);
}

static int _execute_pinned_vectored(const void *obj, hpx_parcel_t *p) {
  void *target;
  if (!hpx_gas_try_pin(p->target, &target)) {
    log_action("pinned action resend.\n");
    return HPX_RESEND;
  }

  const action_entry_t *entry = obj;
  void *args = hpx_parcel_get_data(p);
  int nargs = *(int*)args;
  size_t *sizes = (size_t*)((char*)args + sizeof(int));
  void *argsp[nargs];
  void *vargs = (char*)sizes + (nargs * sizeof(size_t));
  argsp[0] = (char*)vargs + ALIGN(vargs-args, 8);

  for (int i = 0, e = nargs - 1; i < e; ++i) {
    argsp[i + 1] = (char*)argsp[i] + sizes[i] + ALIGN(sizes[i], 8);
  }

  hpx_pinned_vectored_action_handler_t handler =
      (hpx_pinned_vectored_action_handler_t)entry->handler;
  return handler(target, nargs, argsp, sizes);
}

int action_execute(hpx_parcel_t *p) {
  dbg_assert(p->target != HPX_NULL);
  dbg_assert_str(p->action != HPX_ACTION_INVALID, "registration error\n");
  dbg_assert_str(p->action < here->actions->n,
                 "action, %d, out of bounds [0,%u)\n",
                 p->action, here->actions->n);

  hpx_action_t             id = p->action;
  const action_table_t *table = here->actions;
  CHECK_BOUND(table, id);
  const action_entry_t *entry = &table->entries[id];
  return entry->execute(entry, p);
}

void entry_set_execute(action_entry_t *entry) {
  uint32_t attr = entry->attr & ~(HPX_INTERNAL);
  switch (attr) {
   case (HPX_ATTR_NONE):
    entry->execute = _execute_ffi;
    return;
   case (HPX_PINNED):
    entry->execute = _execute_pinned_ffi;
    return;
   case (HPX_MARSHALLED):
    entry->execute = _execute_marshalled;
    return;
   case (HPX_PINNED | HPX_MARSHALLED):
    entry->execute = _execute_pinned_marshalled;
    return;
   case (HPX_MARSHALLED | HPX_VECTORED):
    entry->execute = _execute_vectored;
    return;
   case (HPX_PINNED | HPX_MARSHALLED | HPX_VECTORED):
    entry->execute = _execute_pinned_vectored;
    return;
  }
  dbg_error("Could not find an execution handler for attr %" PRIu32 "\n",
            entry->attr);
}
