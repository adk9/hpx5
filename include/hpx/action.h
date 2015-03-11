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
#ifndef HPX_ACTION_H
#define HPX_ACTION_H

#include "hpx/builtins.h"

/// @file
/// @brief Types and functions for registering HPX actions.

/// The handle type for HPX actions.  This handle is obtained via
/// hpx_register_action(). It is safe to use this handle only after a
/// call to hpx_finalize_action() or after hpx_run(). It is used as a
/// parameter type for any HPX function that needs an action (e.g.
/// hpx_run(), hpx_call(), hpx_parcel_set_action()).
typedef uint16_t hpx_action_t;

/// The type of functions that can be registered with hpx_register_action().
typedef int (*hpx_action_handler_t)(void*);

/// The type of functions that can be registed with pinned actions.
typedef int (*hpx_pinned_action_handler_t)(void *, void*);

/// The equivalent of NULL for HPX actions.
#define HPX_ACTION_NULL ((hpx_action_t)0u)

/// Action types.
typedef enum {
  HPX_ACTION_DEFAULT = 0,
  HPX_ACTION_PINNED,
  HPX_ACTION_TASK,
  HPX_ACTION_INTERRUPT,
  HPX_ACTION_INVALID = UINT16_MAX
} hpx_action_type_t;

static const char* const HPX_ACTION_TYPE_TO_STRING[] = {
  "DEFAULT",
  "PINNED",
  "TASK",
  "INTERRUPT",
  "INVALID"
};

/// Register an HPX action of a given @p type.
///
/// The action could be a regular action, a pinned action, or a task
/// or an interrupt. Actions should be registered from the main thread
/// after calling hpx_init and before calling hpx_run.
///
/// @param    id the action id for this action to be returned after registration
/// @param   key a unique string key for the action
/// @param     f the local function pointer to associate with the action
/// @param nargs the variadic number of arguments that this action accepts
/// @param  type the type of the action to be registered
/// @returns     error code
int hpx_register_action(hpx_action_type_t type, const char *key, hpx_action_handler_t f,
                        unsigned int nargs, hpx_action_t *id, ...);

/// Wraps the hpx_register_action() function to make it slightly more convenient
/// to use.
///
/// @param         type The type of the action (DEFAULT, PINNED, etc...)
/// @param      handler The action handler (the function).
/// @param (car __VA_ARGS__) The action id (the hpx_action_t address)
/// @param (cdr __VA_ARGS__) The parameter types (HPX_INT, ...)
#define _HPX_REGISTER_ACTION(type, handler, ...)                    \
  hpx_register_action(HPX_ACTION_##type, __FILE__":"_HPX_XSTR(handler), \
                      (hpx_action_handler_t)handler,                \
                      __HPX_NARGS(__VA_ARGS__) - 1, __VA_ARGS__)

/// Declare an action.
///
/// This doesn't actually do anything interesting, but if we ever needed to
/// mangle the symbol then we would do it here. This can be prefixed with a
/// storage modifier (i.e., extern, static).
///
/// @param       symbol The symbol for the action.
#define HPX_ACTION_DECL(symbol) hpx_action_t symbol

/// Create an action id for a function, so that it can be called asynchronously.
///
/// @param         type The action type.
/// @param      handler The handler.
/// @param           id The action id.
/// @param  __VA_ARGS__ The action type.
#define HPX_ACTION_DEF(type, handler, id, ...)                      \
  HPX_ACTION_DECL(id) = -1;                                         \
  static HPX_CONSTRUCTOR void _register##_##handler(void) {         \
    _HPX_REGISTER_ACTION(type, handler, &id,##__VA_ARGS__);         \
  }                                                                 \
  static HPX_CONSTRUCTOR void _register##_##handler(void)

/// Define an HPX.
///
/// This will define an action.
///
/// @param         type The type of the action (DEFAULT, PINNED, etc).
/// @param           id The id that you pass to hpx_call to call the action.
/// @param         args The C argument type (must be a pointer type).
#define HPX_ACTION_DEF_USER(type, id, ...)                      \
  HPX_ACTION_DECL(id) = -1;                                     \
  static int id##_##type(__VA_ARGS__);                          \
  static HPX_CONSTRUCTOR void _register_##id##_##type(void) {   \
    _HPX_REGISTER_ACTION(type, id##_##type , &id);              \
  }                                                             \
  static int id##_##type(__VA_ARGS__)

#define HPX_ACTION(id, ...)    HPX_ACTION_DEF_USER(DEFAULT, id, __VA_ARGS__)
#define HPX_PINNED(id, ...)    HPX_ACTION_DEF_USER(PINNED, id, __VA_ARGS__)
#define HPX_TASK(id, ...)      HPX_ACTION_DEF_USER(TASK, id, __VA_ARGS__)
#define HPX_INTERRUPT(id, ...) HPX_ACTION_DEF_USER(INTERRUPT, id, __VA_ARGS__)

#define HPX_REGISTER_ACTION(handler, id)                        \
  _HPX_REGISTER_ACTION(DEFAULT, handler, id)

#define HPX_REGISTER_TYPED_ACTION(type, handler, id, ...)       \
  _HPX_REGISTER_ACTION(type, handler, id, __VA_ARGS__)

#endif
