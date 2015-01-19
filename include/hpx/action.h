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
  HPX_ACTION_INVALID = -1,
  HPX_ACTION_DEFAULT = 0,
  HPX_ACTION_PINNED,
  HPX_ACTION_TASK,
  HPX_ACTION_INTERRUPT,
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



// Register a regular HPX action.
#define HPX_REGISTER_ACTION(f, ...)                                                    \
  hpx_register_action(HPX_ACTION_DEFAULT, _HPX_XSTR(_hpx##f), (hpx_action_handler_t)f, \
                      __HPX_NARGS(__VA_ARGS__)-1, __VA_ARGS__)


/// Register a pinned action. The global address that these actions
/// are addressed to is pinned by the runtime during the course of
/// execution of the action.
#define HPX_REGISTER_PINNED_ACTION(f, ...)                                             \
  hpx_register_action(HPX_ACTION_PINNED, _HPX_XSTR(_hpx##f), (hpx_action_handler_t)f,  \
                      __HPX_NARGS(__VA_ARGS__)-1, __VA_ARGS__)


/// Register an HPX "task". Tasks are non-blocking actions that do not
/// need a stack. They are work-units that are still load-balanced by
/// the scheduler between the available execution units (worker
/// thread) on a locality. Tasks can be stolen, like other HPX
/// threads, but avoid the stack creation overhead since they do not
/// block.
#define HPX_REGISTER_TASK(f, ...)                                                      \
  hpx_register_action(HPX_ACTION_TASK, _HPX_XSTR(_hpx##f), (hpx_action_handler_t)f,    \
                      __HPX_NARGS(__VA_ARGS__)-1, __VA_ARGS__)


/// Register an HPX "interrupt". Interrupts are immediate,
/// non-blocking actions that can safely run in an interrupt context
/// (for instance, doing a memory store operation or performing an
/// asynchronous call). They avoid both, the stack creation overhead
/// and the scheduling overhead, as they are executed inline by the
/// communication thread.
#define HPX_REGISTER_INTERRUPT(f, ...)                                                  \
  hpx_register_action(HPX_ACTION_INTERRUPT, _HPX_XSTR(_hpx##f), (hpx_action_handler_t)f,\
                      __HPX_NARGS(__VA_ARGS__)-1, __VA_ARGS__)


/// A helper macro to declare and define HPX actions.
#define HPX_DEFINE_ACTION(type, action)             \
  static int action##_##type(void*);                \
  static hpx_action_t action = 0;                   \
  static HPX_CONSTRUCTOR                            \
  void _register_##action##_##type(void) {          \
    HPX_REGISTER_##type(action##_##type, &action);  \
  }                                                 \
  static int action##_##type

#define HPX_ACTION(n)        HPX_DEFINE_ACTION(ACTION, n)
#define HPX_PINNED_ACTION(n) HPX_DEFINE_ACTION(PINNED_ACTION, n)
#define HPX_TASK(n)          HPX_DEFINE_ACTION(TASK, n)
#define HPX_INTERRUPT(n)     HPX_DEFINE_ACTION(INTERRUPT, n)


#endif
