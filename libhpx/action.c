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
#include <stdarg.h>
#include <string.h>

#include <hpx/builtins.h>
#include <hpx/types.h>
#include <libhpx/action.h>
#include <libhpx/parcel.h>
#include <libhpx/debug.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/padding.h>
#include <libhpx/percolation.h>
#include <libhpx/utils.h>

/// The default libhpx action table size.
#define LIBHPX_ACTION_TABLE_SIZE 4096

/// An action table entry type.
///
/// This stores information associated with an HPX action. In
/// particular, an action entry maintains the action handler
/// (function), a globally unique string key, a unique id generated
/// during action finalization, action types and attributes, e.g., can
/// they block, should we pre-pin their arguments, etc., and an
/// environment pointer.
///
typedef struct {
  hpx_action_handler_t handler;
  hpx_action_t             *id;
  const char              *key;
  hpx_action_type_t       type;
  uint32_t                attr;
  ffi_cif                 *cif;
  void                    *env;
} _entry_t;

/// Compare two entries by their keys.
///
/// This is used to sort the action table during finalization, so that we can
/// uniformly assign ids to actions independent of which region in the local
/// address space the functions are loaded into.
///
/// @param          lhs A pointer to the left-hand entry.
/// @param          rhs A pointer tot he right-hand entry.
///
/// @return             The lexicographic comparison of the entries' keys.
static int _cmp_keys(const void *lhs, const void *rhs) {
  const _entry_t *el = lhs;
  const _entry_t *er = rhs;

  // if either the left or right entry's id is NULL, that means it is
  // our reserved action (user-registered actions can never have the
  // ID as NULL), and we always want it to be "less" than any other
  // registered action.
  if (el->id == NULL) {
    return -1;
  } else if (er->id == NULL) {
    return 1;
  } else {
    return strcmp(el->key, er->key);
  }
}

/// An action table is simply an array that stores its size.
///
///
typedef struct action_table {
  int n;
  _entry_t entries[];
} _table_t;

/// A static action table.
///
/// We currently need to be able to register actions before we call hpx_init()
/// because we use constructors inside of libhpx to do action registration. We
/// expose this action table to be used for that purpose.
static _table_t *_actions = NULL;

/// Get the static action table.
///
/// This is not synchronized and thus unsafe to call in a multithreaded
/// environment, but we make sure to call it in hpx_init() where we assume we
/// are running in single-threaded mode, so we should be safe.
static _table_t *_get_actions(void) {
  if (!_actions) {
    int bytes = sizeof(_table_t) + LIBHPX_ACTION_TABLE_SIZE * sizeof(_entry_t);
    _actions = calloc(1, bytes);
    _actions->n = 1;
  }

  return _actions;
}

/// Sort the actions in an action table by their key.
static void _sort_entries(_table_t *table) {
  qsort(&table->entries, table->n, sizeof(_entry_t), _cmp_keys);
}

/// Assign all of the entry ids in the table.
static void _assign_ids(_table_t *table) {
  for (int i = 1, e = table->n; i < e; ++i) {
    *table->entries[i].id = i;
  }
}

/// Insert an action into a table.
///
/// @param        table The table we are inserting into.
/// @param           id The address of the user's id; written in _assign_ids().
/// @param          key The unique key for this action; read in _sort_entries().
/// @param            f The handler for this action.
/// @param         type The type of this action.
/// @param         attr The attributes associated with this action.
/// @param          cif FFI datatype information.
/// @param          env Action's environment.
///
/// @return             HPX_SUCCESS or an error if the push fails.
static int _push_back(_table_t *table, hpx_action_t *id, const char *key,
                      hpx_action_handler_t f, hpx_action_type_t type,
                      uint32_t attr, ffi_cif *cif, void *env) {
  int i = table->n++;
  if (LIBHPX_ACTION_TABLE_SIZE < i) {
    dbg_error("action table overflow\n");
  }
  _entry_t *back = &table->entries[i];
  back->handler = f;
  back->id = id;
  back->key = key;
  back->type = type;
  back->attr = attr;
  back->cif = cif;
  back->env = env;
  return HPX_SUCCESS;
}

const _table_t *action_table_finalize(void) {
  _table_t *table = _get_actions();
  _sort_entries(table);
  _assign_ids(table);

  for (int i = 1, e = table->n; i < e; ++i) {
    const char *key = table->entries[i].key;
    hpx_action_type_t type = table->entries[i].type;
    hpx_action_handler_t f = table->entries[i].handler;
    (void) key, (void) type, (void) f;

#ifdef HAVE_PERCOLATION
    if (here->percolation && type == HPX_OPENCL) {
      void *env = percolation_prepare(here->percolation, key, (const char*)f);
      dbg_assert_str(env, "failed to prepare percolation kernel: %s\n", key);
      table->entries[i].handler =
        (hpx_action_handler_t)percolation_execute_handler;
    }
#endif

    log_action("%d: %s (%p) %s %x.\n", *table->entries[i].id,
               key, (void*)(uintptr_t)f, HPX_ACTION_TYPE_TO_STRING[type],
               table->entries[i].attr);
  }

  // this is a sanity check to ensure that the reserved "null" action
  // is still at index 0.
  dbg_assert(table->entries[0].id == NULL);
  return table;
}

void action_table_free(const _table_t *table) {
  for (int i = 0, e = table->n; i < e; ++i) {
    ffi_cif *cif = table->entries[i].cif;
    if (cif) {
      free(cif->arg_types);
      free(cif);
    }

#ifdef HAVE_PERCOLATION
    void *env = table->entries[i].env;
    if (env && table->entries[i].type == HPX_OPENCL) {
      percolation_destroy(here->percolation, env);
    }
#endif
  }
  free((void*)table);
}

#define _ACTION_TABLE_GET(type, name, init)                             \
  type action_table_get_##name(const struct action_table *table,        \
                               hpx_action_t id) {                       \
    if (id == HPX_ACTION_INVALID) {                                     \
      log_dflt("action registration is not complete");                  \
      return (type)init;                                                \
    } else if (id >= table->n) {                                        \
      dbg_error("action id, %d, out of bounds [0,%u)\n", id, table->n); \
    }                                                                   \
    return table->entries[id].name;                                     \
  }                                                                     \
  type action_table_get_##name(const struct action_table *table,        \
                               hpx_action_t id)

_ACTION_TABLE_GET(const char *, key, NULL);
_ACTION_TABLE_GET(hpx_action_type_t, type, HPX_ACTION_INVALID);
_ACTION_TABLE_GET(uint32_t, attr, 0);
_ACTION_TABLE_GET(hpx_action_handler_t, handler, NULL);
_ACTION_TABLE_GET(ffi_cif *, cif, NULL);
_ACTION_TABLE_GET(void *, env, NULL);

int action_table_size(const struct action_table *table) {
  return table->n;
}

hpx_parcel_t *action_pack_args(hpx_parcel_t *p, int nargs, va_list *vargs) {
  dbg_assert(p);

  if (!nargs) {
    return p;
  }

  if (p->action == HPX_ACTION_NULL) {
    dbg_error("parcel must have an action to serialize arguments.\n");
  }

  const _table_t     *actions = _get_actions();
  const ffi_cif          *cif = action_table_get_cif(actions, p->action);
  const char             *key = action_table_get_key(actions, p->action);
  uint32_t               attr = action_table_get_attr(actions, p->action);

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

  const _table_t *actions = _get_actions();
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
    const ffi_cif *cif = action_table_get_cif(actions, action);
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


int action_call_va(hpx_addr_t addr, hpx_action_t action, hpx_addr_t c_addr,
                   hpx_action_t c_action, hpx_addr_t lsync, hpx_addr_t gate,
                   int nargs, va_list *args) {
  hpx_parcel_t *p = action_create_parcel_va(addr, action, c_addr, c_action,
                                            nargs, args);

  if (likely(!gate && !lsync)) {
    parcel_launch(p);
    return HPX_SUCCESS;
  }
  if (!gate && lsync) {
    return hpx_parcel_send(p, lsync);
  }
  if (!lsync) {
    return hpx_parcel_send_through_sync(p, gate);
  }
  return hpx_parcel_send_through(p, gate, lsync);
}

int action_execute(hpx_parcel_t *p) {
  dbg_assert(p->target != HPX_NULL);
  dbg_assert_str(p->action != HPX_ACTION_INVALID, "registration error\n");
  dbg_assert_str(p->action < _get_actions()->n,
                 "action, %d, out of bounds [0,%u)\n",
                 p->action, _get_actions()->n);

  hpx_action_t              id = p->action;
  const _table_t        *table = _get_actions();
  hpx_action_handler_t handler = table->entries[id].handler;
  dbg_assert(handler);
  ffi_cif                 *cif = table->entries[id].cif;
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

bool action_is_pinned(const struct action_table *table, hpx_action_t id) {
  return (action_table_get_attr(table, id) & HPX_PINNED);
}

bool action_is_marshalled(const struct action_table *table, hpx_action_t id) {
  return (action_table_get_attr(table, id) & HPX_MARSHALLED);
}

bool action_is_vectored(const struct action_table *table, hpx_action_t id) {
  return (action_table_get_attr(table, id) & HPX_VECTORED);
}

bool action_is_internal(const struct action_table *table, hpx_action_t id) {
  return (action_table_get_attr(table, id) & HPX_INTERNAL);
}

bool action_is_default(const struct action_table *table, hpx_action_t id) {
  return (action_table_get_type(table, id) == HPX_DEFAULT);
}

bool action_is_task(const struct action_table *table, hpx_action_t id) {
  return (action_table_get_type(table, id) == HPX_TASK);
}

bool action_is_interrupt(const struct action_table *table, hpx_action_t id) {
  return (action_table_get_type(table, id) == HPX_INTERRUPT);
}

bool action_is_function(const struct action_table *table, hpx_action_t id) {
  return (action_table_get_type(table, id) == HPX_FUNCTION);
}

bool action_is_opencl(const struct action_table *table, hpx_action_t id) {
  return (action_table_get_type(table, id) == HPX_OPENCL);
}

static int
_register_action_va(hpx_action_type_t type, uint32_t attr,
                    const char *key, hpx_action_t *id, hpx_action_handler_t f,
                    int system, unsigned int nargs, va_list vargs) {
  dbg_assert(id);
  *id = HPX_ACTION_INVALID;

  if (system) {
    attr |= HPX_INTERNAL;
  }

  bool marshalled = attr & HPX_MARSHALLED;
  bool     pinned = attr & HPX_PINNED;
  bool   vectored = attr & HPX_VECTORED;

  if (marshalled) {
    if (pinned) {
      hpx_type_t translated = va_arg(vargs, hpx_type_t);
      dbg_assert(translated == HPX_POINTER);
      (void)translated;
    }

    if (vectored) {
      hpx_type_t count = va_arg(vargs, hpx_type_t);
      hpx_type_t args = va_arg(vargs, hpx_type_t);
      hpx_type_t sizes = va_arg(vargs, hpx_type_t);

      dbg_assert(count == HPX_INT || count == HPX_UINT || count == HPX_SIZE_T);
      dbg_assert(args == HPX_POINTER);
      dbg_assert(sizes == HPX_POINTER);

      (void)count;
      (void)args;
      (void)sizes;
    } else {
      hpx_type_t addr = va_arg(vargs, hpx_type_t);
      hpx_type_t size = va_arg(vargs, hpx_type_t);

      dbg_assert(addr == HPX_POINTER);
      dbg_assert(size == HPX_INT || size == HPX_UINT || size == HPX_SIZE_T);
      (void)addr;
      (void)size;
    }

    va_end(vargs);
    return _push_back(_get_actions(), id, key, f, type, attr, NULL, NULL);
  }

  ffi_cif *cif = calloc(1, sizeof(*cif));
  dbg_assert(cif);

  hpx_type_t *args = calloc(nargs, sizeof(args[0]));
  for (int i = 0; i < nargs; ++i) {
    args[i] = va_arg(vargs, hpx_type_t);
  }

  ffi_status s = ffi_prep_cif(cif, FFI_DEFAULT_ABI, nargs, HPX_INT, args);
  if (s != FFI_OK) {
    dbg_error("failed to process type information for action id %d.\n", *id);
  }
  return _push_back(_get_actions(), id, key, f, type, attr, cif, NULL);
}

int
libhpx_register_action(hpx_action_type_t type, uint32_t attr, const char *key,
                       hpx_action_t *id, hpx_action_handler_t f,
                       unsigned int nargs, ...) {
  va_list vargs;
  va_start(vargs, nargs);
  int e = _register_action_va(type, attr, key, id, f, 1, nargs, vargs);
  va_end(vargs);
  return e;
}

int
hpx_register_action(hpx_action_type_t type, uint32_t attr, const char *key,
                    hpx_action_t *id, hpx_action_handler_t f,
                    unsigned int nargs, ...) {
  va_list vargs;
  va_start(vargs, nargs);
  int e = _register_action_va(type, attr, key, id, f, 0, nargs, vargs);
  va_end(vargs);
  return e;
}

hpx_action_handler_t hpx_action_get_handler(hpx_action_t id) {
  return action_table_get_handler(here->actions, id);
}
