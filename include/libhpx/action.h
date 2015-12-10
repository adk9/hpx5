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

#ifndef LIBHPX_ACTION_H
#define LIBHPX_ACTION_H

#include <stdarg.h>
#include <hpx/hpx.h>

///
typedef void (*handler_t)(void);


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
  int           (*execute_parcel)(const void *obj, hpx_parcel_t *buffer);
  void          (*pack_buffer)(const void *obj, void *buffer, int n, va_list *args);
  hpx_parcel_t *(*new_parcel)(const void *obj, hpx_addr_t addr,
                              hpx_addr_t c_addr, hpx_action_t c_action,
                              int n, va_list *args);
  handler_t      handler;
  hpx_action_t       *id;
  const char        *key;
  hpx_action_type_t type;
  uint32_t          attr;
  ffi_cif           *cif;
  void              *env;
} action_entry_t;

/// The default libhpx action table size.
#define LIBHPX_ACTION_MAX (UINT32_C(1) << (sizeof(hpx_action_t) * 8))

extern action_entry_t actions[LIBHPX_ACTION_MAX];

#ifdef ENABLE_DEBUG
void CHECK_BOUND(const action_entry_t *table, hpx_action_t id);
#else
#define CHECK_BOUND(table, id)
#endif

/// Register an HPX action of a given @p type. This is similar to the
/// hpx_register_action routine, except that it gives us the chance to "tag"
/// LIBHPX actions in their own way. This can be useful for instrumentation or
/// optimization later. This must be called before hpx_init().
///
/// @param  type The type of the action to be registered.
/// @param  attr The attribute of the action (PINNED, PACKED, ...).
/// @param   key A unique string key for the action.
/// @param     f The local function pointer to associate with the action.
/// @param    id The action id for this action to be returned after
///                registration.
/// @param nargs The variadic number of parameters that the action accepts.
/// @param   ... The HPX types of the action parameters (HPX_INT, ...).
///
/// @returns     HPX_SUCCESS or an error code
int libhpx_register_action(hpx_action_type_t type, uint32_t attr,
                           const char *key, hpx_action_t *id, void (*f)(void),
                           unsigned nargs, ...);


/// Called when all of the actions have been registered.
void action_table_complete(void);

/// Called to free any internal data allocated by the actions.
void action_table_finalize(void);


/// Report the number of actions registerd in the table
int action_table_size(void);

/// Run the handler associated with an action.
int action_execute(struct hpx_parcel *)
  HPX_NON_NULL(1);

/// Serialize the vargs into the parcel.
hpx_parcel_t *action_pack_args(hpx_parcel_t *p, int nargs, va_list *vargs);

/// Returns a parcel that encodes the target address, an action and
/// its argument, and the continuation. The parcel is ready to be sent
/// to effect a call operation.
hpx_parcel_t *action_create_parcel_va(hpx_addr_t addr, hpx_action_t action,
                                      hpx_addr_t c_addr, hpx_action_t c_action,
                                      int nargs, va_list *args);

/// Same as above, with the exception that the input arguments are
/// variadic instead of a va_list.
hpx_parcel_t *action_create_parcel(hpx_addr_t addr, hpx_action_t action,
                                   hpx_addr_t c_addr, hpx_action_t c_action,
                                   int nargs, ...);

/// Call an action by sending a parcel given a list of variable args.
int action_call_va(hpx_addr_t addr, hpx_action_t action, hpx_addr_t c_addr,
                   hpx_action_t c_action, hpx_addr_t lsync, hpx_addr_t gate,
                   int nargs, va_list *args);

static inline bool entry_is_pinned(const action_entry_t *e) {
  return (e->attr & HPX_PINNED);
}

static inline bool entry_is_marshalled(const action_entry_t *e) {
  return (e->attr & HPX_MARSHALLED);
}

static inline bool entry_is_vectored(const action_entry_t *e) {
  return (e->attr & HPX_VECTORED);
}

static inline bool entry_is_internal(const action_entry_t *e) {
  return (e->attr & HPX_INTERNAL);
}

static inline bool entry_is_default(const action_entry_t *e) {
  return (e->type == HPX_DEFAULT);
}

static inline bool entry_is_task(const action_entry_t *e) {
  return (e->type == HPX_TASK);
}

static inline bool entry_is_interrupt(const action_entry_t *e) {
  return (e->type == HPX_INTERRUPT);
}

static inline bool entry_is_function(const action_entry_t *e) {
  return (e->type == HPX_FUNCTION);
}

static inline bool entry_is_opencl(const action_entry_t *e) {
  return (e->type == HPX_OPENCL);
}

static inline bool action_is_pinned(hpx_action_t id) {
  CHECK_BOUND(actions, id);
  const action_entry_t *entry = &actions[id];
  return entry_is_pinned(entry);
}

static inline bool action_is_marshalled(hpx_action_t id) {
  CHECK_BOUND(actions, id);
  const action_entry_t *entry = &actions[id];
  return entry_is_marshalled(entry);
}

static inline bool action_is_vectored(hpx_action_t id) {
  CHECK_BOUND(actions, id);
  const action_entry_t *entry = &actions[id];
  return entry_is_vectored(entry);
}

static inline bool action_is_internal(hpx_action_t id) {
  CHECK_BOUND(actions, id);
  const action_entry_t *entry = &actions[id];
  return entry_is_internal(entry);
}

static inline bool action_is_default(hpx_action_t id) {
  CHECK_BOUND(actions, id);
  const action_entry_t *entry = &actions[id];
  return entry_is_default(entry);
}

static inline bool action_is_task(hpx_action_t id) {
  CHECK_BOUND(actions, id);
  const action_entry_t *entry = &actions[id];
  return entry_is_task(entry);
}

static inline bool action_is_interrupt(hpx_action_t id) {
  CHECK_BOUND(actions, id);
  const action_entry_t *entry = &actions[id];
  return entry_is_interrupt(entry);
}

static inline bool action_is_function(hpx_action_t id) {
  CHECK_BOUND(actions, id);
  const action_entry_t *entry = &actions[id];
  return entry_is_function(entry);
}

static inline bool action_is_opencl(hpx_action_t id) {
  CHECK_BOUND(actions, id);
  const action_entry_t *entry = &actions[id];
  return entry_is_opencl(entry);
}

/// Wraps the libhpx_register_action() function to make it slightly
/// more convenient to use.
///
/// @param        type The type of the action (THREAD, TASK, INTERRUPT, ...).
/// @param        attr The attribute of the action (PINNED, PACKED, ...).
/// @param     handler The action handler (the function).
/// @param          id The action id (the hpx_action_t address).
/// @param __VA_ARGS__ The parameter types (HPX_INT, ...).
#define LIBHPX_REGISTER_ACTION(type, attr, id, handler, ...)          \
  libhpx_register_action(type, attr, __FILE__ ":" _HPX_XSTR(id),      \
                         &id, (handler_t)handler,          \
                         __HPX_NARGS(__VA_ARGS__) , ##__VA_ARGS__)

/// Create an action id for a function, so that it can be called asynchronously.
///
/// This macro handles all steps of creating a usable action. It declares the
/// identifier and registers an action in a static constructor.  The static
/// automates action registration, eliminating the need for explicit action
/// registration.
///
/// Note that the macro can be preceded by the \c static keyword if the action
/// should only be visible in the current file.  This macro is intended to be
/// used only for actions that are part of libhpx.
///
/// @param         type The action type.
/// @param         attr The action attributes.
/// @param      handler The handler.
/// @param           id The action id.
/// @param  __VA_ARGS__ The HPX types of the action paramters
///                     (HPX_INT, ...).
#define LIBHPX_ACTION(type, attr, id, handler, ...)                  \
  HPX_ACTION_DECL(id) = HPX_ACTION_INVALID;                          \
  static HPX_CONSTRUCTOR void _register##_##id(void) {               \
    LIBHPX_REGISTER_ACTION(type, attr, id, handler , ##__VA_ARGS__); \
  }                                                                  \
  static HPX_CONSTRUCTOR void _register##_##id(void)

#endif // LIBHPX_ACTION_H
