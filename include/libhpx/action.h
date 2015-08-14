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
struct action_table;
struct hpx_parcel;

/// Register an HPX action of a given @p type. This is similar to the
/// hpx_register_action routine, except that it gives us the chance to "tag"
/// LIBHPX actions in their own way. This can be useful for instrumentation or
/// optimization later.
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
                           const char *key, hpx_action_t *id,
                           hpx_action_handler_t f, unsigned int nargs, ...);


/// Get the key for an action.
const char *action_table_get_key(const struct action_table *, hpx_action_t)
  HPX_NON_NULL(1);

/// Get the action type.
hpx_action_type_t action_table_get_type(const struct action_table *,
                                        hpx_action_t)
  HPX_NON_NULL(1);

/// Get the key for an action.
hpx_action_handler_t action_table_get_handler(const struct action_table *,
                                              hpx_action_t)
  HPX_NON_NULL(1);

/// Report the number of actions registerd in the table
int action_table_size(const struct action_table *table);

/// Run the handler associated with an action.
int action_execute(struct hpx_parcel *)
  HPX_NON_NULL(1);

/// Get the FFI type information associated with an action.
ffi_cif *action_table_get_cif(const struct action_table *, hpx_action_t)
  HPX_NON_NULL(1);

/// Serialize the vargs into the parcel.
hpx_parcel_t *action_pack_args(hpx_parcel_t *p, int n, va_list *vargs);

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

/// Is the action a pinned action?
bool action_is_pinned(const struct action_table *, hpx_action_t)
  HPX_NON_NULL(1);

/// Is the action a marshalled action?
bool action_is_marshalled(const struct action_table *, hpx_action_t)
  HPX_NON_NULL(1);

/// Is the action internal?
bool action_is_internal(const struct action_table *, hpx_action_t)
  HPX_NON_NULL(1);

/// Is the action a default action?
bool action_is_default(const struct action_table *, hpx_action_t)
  HPX_NON_NULL(1);

/// Is the action a task?
bool action_is_task(const struct action_table *, hpx_action_t)
  HPX_NON_NULL(1);

/// Is the action an interrupt?
bool action_is_interrupt(const struct action_table *, hpx_action_t)
  HPX_NON_NULL(1);

/// Is the action a function?
bool action_is_function(const struct action_table *, hpx_action_t)
  HPX_NON_NULL(1);

/// Build an action table.
///
/// This will process all of the registered actions, sorting them by key and
/// assigning ids to their registered id addresses. The caller obtains ownership
/// of the table and must call action_table_free() to release its resources.
///
/// @return             An action table that can be indexed by the keys
///                     originally registered.
const struct action_table *action_table_finalize(void);

/// Free an action table.
void action_table_free(const struct action_table *action)
  HPX_NON_NULL(1);

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
                         &id, (hpx_action_handler_t)handler,          \
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
