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

/// @defgroup actions Actions and threads
/// @brief Functions and definitions for registering, calling, and controlling
///        actions and threads
/// @{

/// @file
/// @brief Types and functions for registering HPX actions.

/// The handle type for HPX actions.  This handle is obtained via
/// hpx_register_action(). It is safe to use this handle only after a
/// call to hpx_finalize_action() or after hpx_run(). It is used as a
/// parameter type for any HPX function that needs an action (e.g.
/// hpx_run(), hpx_call(), hpx_parcel_set_action()).
typedef uint16_t hpx_action_t;

/// The type of functions that can be registered with hpx_register_action().
typedef int (*hpx_action_handler_t)(size_t, void*);

/// The type of functions that can be registed with pinned actions.
typedef int (*hpx_pinned_action_handler_t)(void *, size_t, void*);

/// The equivalent of NULL for HPX actions.
#define HPX_ACTION_NULL ((hpx_action_t)0u)

/// An invalid action
#define HPX_ACTION_INVALID ((hpx_action_t)UINT16_MAX)

/// Action types.
typedef enum {
  HPX_DEFAULT = 0,
  HPX_TASK,
  HPX_INTERRUPT,
  HPX_FUNCTION,
} hpx_action_type_t;

static const char* const HPX_ACTION_TYPE_TO_STRING[] = {
  "DEFAULT",
  "TASK",
  "INTERRUPT",
  "FUNCTION",
};

/// Action attributes.
#define HPX_MARSHALLED 0x1
#define HPX_PINNED     0x2

/// Register an HPX action of a given @p type.
///
/// @param  type The type of the action to be registered.
/// @param  attr The attribute of the action (PINNED, PACKED, ...).
/// @param   key A unique string key for the action.
/// @param     f The local function pointer to associate with the action.
/// @param    id The action id for this action to be returned after registration.
/// @param nargs The variadic number of parameters that the action accepts.
/// @param   ... The HPX types of the action parameters (HPX_INT, ...).
/// @returns     Error code
int hpx_register_action(hpx_action_type_t type, uint32_t attr,
                        const char *key, hpx_action_t *id,
                        hpx_action_handler_t f, unsigned int nargs, ...);

/// Wraps the hpx_register_typed_action() function to make it slightly
/// more convenient to use.
///
/// @param        type The type of the action (THREAD, TASK, INTERRUPT, ...).
/// @param        attr The attribute of the action (PINNED, PACKED, ...).
/// @param     handler The action handler (the function).
/// @param          id The action id (the hpx_action_t address).
/// @param __VA_ARGS__ The parameter types (HPX_INT, ...).
#define HPX_REGISTER_ACTION(type, attr, id, handler, ...)        \
  hpx_register_action(type, attr, __FILE__":"_HPX_XSTR(handler), \
                      &id, (hpx_action_handler_t)handler,        \
                      __HPX_NARGS(__VA_ARGS__) , ##__VA_ARGS__)

/// Get the handler associated with a given action id.
hpx_action_handler_t hpx_action_get_handler(hpx_action_t id);

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
/// @param         attr The action attributes.
/// @param      handler The handler.
/// @param           id The action id.
/// @param  __VA_ARGS__ The HPX types of the action paramters
///                     (HPX_INT, ...).
#define HPX_ACTION(type, attr, id, handler, ...)                  \
  HPX_ACTION_DECL(id) = HPX_ACTION_INVALID;                       \
  static HPX_CONSTRUCTOR void _register##_##handler(void) {       \
    HPX_REGISTER_ACTION(type, attr, id, handler , ##__VA_ARGS__); \
  }                                                               \
  static HPX_CONSTRUCTOR void _register##_##handler(void)

/// @}

#endif
