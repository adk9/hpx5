// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013, Trustees of Indiana University,
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

/// This special action does nothing (i.e. it is a nop).
extern hpx_action_t HPX_ACTION_NULL;

/// Action types.
typedef enum {
  HPX_ACTION_DEFAULT = 0,
  HPX_ACTION_PINNED,
  HPX_ACTION_TASK,
  HPX_ACTION_INTERRUPT,
  HPX_ACTION_INVALID = UINT16_MAX
} hpx_action_type_t;



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

/// Miscellaneous utility macros.

#define _HPX_XSTR(s) _HPX_STR(s)
#define _HPX_STR(l) #l


/// Macro to count the number of variadic arguments
/// Source: https://groups.google.com/forum/#!topic/comp.std.c/d-6Mj5Lko_s

#define __HPX_NARGS(...) _HPX_NARGS(__VA_ARGS__,63,62,61,60,59,58,57,56,55,54,53,52,51,50,   \
  49,48,47,46,45,44,43,42,41,40,39,38,37,36,35,34,33,32,31,30,29,28,27,26,25,24,23,22,21,20, \
  19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0)
#define _HPX_NARGS(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,   \
  _21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,_33,_34,_35,_36,_37,_38,_39,_40,_41,_42,   \
  _43,_44,_45,_46,_47,_48,_49,_50,_51,_52,_53,_54,_55,_56,_57,_58,_59,_60,_61,_62,_63,_,...) _

/// Wraps the hpx_register_action() function to make it slightly more convenient
/// to use.
///
/// @param         type The type of the action (DEFAULT, PINNED, etc...)
/// @param      handler The action handler (the function).
/// @param (car __VA_ARGS__) The action id (the hpx_action_t address)
/// @param (cdr __VA_ARGS__) The parameter types (HPX_INT, ...)
#define _HPX_REGISTER_ACTION(type, handler, ...)                    \
  hpx_register_action(HPX_ACTION_##type, _HPX_XSTR(_id##handler),   \
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
    _HPX_REGISTER_ACTION(type, handler, &id, __VA_ARGS__);          \
  }                                                                 \
  static HPX_CONSTRUCTOR void _register##_##handler(void)

/// Define an HPX.
///
/// This will define an action.
///
/// @param         type The type of the action (DEFAULT, PINNED, etc).
/// @param           id The id that you pass to hpx_call to call the action.
/// @param         args The C argument type (must be a pointer type).
#define HPX_ACTION_DEF_USER(type, id, args)                     \
  HPX_ACTION_DECL(id) = -1;                                     \
  static int id##_##type(args);                                 \
  static HPX_CONSTRUCTOR void _register_##id##_##type(void) {   \
    _HPX_REGISTER_ACTION(type, id##_##type , &id);              \
  }                                                             \
  static int id##_##type(args)


#define HPX_ACTION(id, args)    HPX_ACTION_DEF_USER(DEFAULT, id, args)
#define HPX_PINNED(id, args)    HPX_ACTION_DEF_USER(PINNED, id, args)
#define HPX_TASK(id, args)      HPX_ACTION_DEF_USER(TASK, id, args)
#define HPX_INTERRUPT(id, args) HPX_ACTION_DEF_USER(INTERRUPT, id, args)

#define HPX_REGISTER_ACTION(handler, id) \
  _HPX_REGISTER_ACTION(DEFAULT, handler, id)

#endif
