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

#include <stdlib.h>
#include <hpx/hpx.h>
#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libhpx/parcel.h>
#include "init.h"
#include "exit.h"

static void _pack_ffi_0(const void *o, hpx_parcel_t *p, int n, va_list *args) {
}

static void _pack_ffi_n(const void *obj, hpx_parcel_t *p, int n, va_list *args)
{
  const action_t *action = obj;
  const ffi_cif *cif = action->cif;
  void *buffer = hpx_parcel_get_data(p);

  DEBUG_IF (n != cif->nargs) {
    const char *key = action->key;
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
  ffi_ptrarray_to_raw((void*)cif, argps, buffer);
}

static void _pack_pinned_ffi_n(const void *obj, hpx_parcel_t *p, int n,
                               va_list *args) {
  const action_t *action = obj;
  const ffi_cif *cif = action->cif;
  void *buffer = hpx_parcel_get_data(p);

  DEBUG_IF (n + 1 != cif->nargs) {
    const char *key = action->key;
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
  ffi_ptrarray_to_raw((void*)cif, argps, buffer);
}

static hpx_parcel_t *_new_ffi_0(const void *obj, hpx_addr_t addr,
                                hpx_addr_t c_addr, hpx_action_t c_action,
                                int n, va_list *args) {
  const action_t *action = obj;
  hpx_action_t id = *action->id;
  hpx_pid_t pid = hpx_thread_current_pid();
  return parcel_new(addr, id, c_addr, c_action, pid, NULL, 0);
}

static hpx_parcel_t *_new_ffi_n(const void *obj, hpx_addr_t addr,
                                hpx_addr_t c_addr, hpx_action_t c_action,
                                int n, va_list *args) {
  const action_t *action = obj;
  hpx_action_t id = *action->id;
  hpx_pid_t pid = hpx_thread_current_pid();
  size_t bytes = ffi_raw_size(action->cif);
  hpx_parcel_t *p = parcel_new(addr, id, c_addr, c_action, pid, NULL, bytes);
  _pack_ffi_n(obj, p, n, args);
  return p;
}

static hpx_parcel_t *_new_pinned_ffi_n(const void *obj, hpx_addr_t addr,
                                       hpx_addr_t c_addr, hpx_action_t c_action,
                                       int n, va_list *args) {
  const action_t *action = obj;
  hpx_action_t id = *action->id;
  hpx_pid_t pid = hpx_thread_current_pid();
  size_t bytes = ffi_raw_size(action->cif);
  hpx_parcel_t *p = parcel_new(addr, id, c_addr, c_action, pid, NULL, bytes);
  _pack_pinned_ffi_n(obj, p, n, args);
  return p;
}

static int _exec_ffi_n(const void *obj, hpx_parcel_t *p) {
  const action_t *action = obj;
  char ffiret[8];               // https://github.com/atgreen/libffi/issues/35
  int *ret = (int*)&ffiret[0];
  void *args = hpx_parcel_get_data(p);
  ffi_raw_call(action->cif, action->handler, ret, args);
  return *ret;
}

static int _exec_pinned_ffi_n(const void *obj, hpx_parcel_t *p) {
  void *target;
  if (!hpx_gas_try_pin(p->target, &target)) {
    log_action("pinned action resend.\n");
    return HPX_RESEND;
  }

  const action_t *action = obj;
  ffi_cif *cif = action->cif;
  void *args = hpx_parcel_get_data(p);
  void *avalue[cif->nargs];
  ffi_raw_to_ptrarray(cif, args, avalue);
  avalue[0] = &target;
  char ffiret[8];               // https://github.com/atgreen/libffi/issues/35
  int *ret = (int*)&ffiret[0];
  ffi_call(cif, action->handler, ret, avalue);
  hpx_gas_unpin(p->target);
  return *ret;
}

static void _ffi_finish(void *act) {
  action_t *action = act;
  log_action("%d: %s (%p) %s %x.\n", *action->id, action->key,
             (void*)(uintptr_t)action->handler,
             HPX_ACTION_TYPE_TO_STRING[action->type],
             action->attr);
}

static void _ffi_fini(void *act) {
  action_t *action = act;
  free(action->cif->arg_types);
  free(action->cif);
}

static const parcel_management_vtable_t _ffi_0_vtable = {
  .new_parcel = _new_ffi_0,
  .pack_parcel = _pack_ffi_0,
  .exec_parcel = _exec_ffi_n,
  .exit = exit_action
};

static const parcel_management_vtable_t _pinned_ffi_0_vtable = {
  .new_parcel = _new_ffi_0,
  .pack_parcel = _pack_ffi_0,
  .exec_parcel = _exec_pinned_ffi_n,
  .exit = exit_pinned_action
};

static const parcel_management_vtable_t _ffi_n_vtable = {
  .new_parcel = _new_ffi_n,
  .pack_parcel = _pack_ffi_n,
  .exec_parcel = _exec_ffi_n,
  .exit = exit_action
};

static const parcel_management_vtable_t _pinned_ffi_n_vtable = {
  .new_parcel = _new_pinned_ffi_n,
  .pack_parcel = _pack_ffi_n,
  .exec_parcel = _exec_pinned_ffi_n,
  .exit = exit_pinned_action
};

void action_init_ffi(action_t *action, int n, va_list *vargs) {
  // Translate the argument types into a stack allocated buffer suitable for use
  // with ffi.
  hpx_type_t *args = calloc(n, sizeof(args[0]));
  for (int i = 0; i < n; ++i) {
    args[i] = va_arg(vargs, hpx_type_t);
  }

  // Check to make sure that pinned actions start with a pointer type.
  uint32_t pinned = action->attr & HPX_PINNED;
  if (pinned && (args[0] != HPX_POINTER)) {
    dbg_error("First type of a pinned action should be HPX_POINTER\n");
  }

  // Allocate and initialize an ffi_cif, which is the structure that ffi uses to
  // encode calling conventions.
  action->cif = calloc(1, sizeof(ffi_cif));
  dbg_assert(action->cif);

  ffi_status s = ffi_prep_cif(action->cif, FFI_DEFAULT_ABI, n, HPX_INT, args);
  if (s != FFI_OK) {
    dbg_error("failed to process type information for action id %d.\n",
              *action->id);
  }

  // Initialize the parcel class.
  if (pinned && action->cif->nargs > 1) {
    action->parcel_class = &_pinned_ffi_n_vtable;
  }
  else if (pinned) {
    action->parcel_class = &_pinned_ffi_0_vtable;
  }
  else if (action->cif->nargs) {
    action->parcel_class = &_ffi_n_vtable;
  }
  else {
    action->parcel_class = &_ffi_0_vtable;
  }

  // Initialize the action class.
  action_init_call_by_parcel(action);

  // Initialize the destructor.
  action->fini = _ffi_fini;
  action->finish = _ffi_finish;
}
