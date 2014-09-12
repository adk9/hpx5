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
#ifndef HPX_PAR_H
#define HPX_PAR_H

/// @file
/// @brief HPX parallel loop interface


int
hpx_par_for(int (*fn)(const int, const void*), const int min, const int max,
            const void *args, hpx_addr_t sync);

int
hpx_par_for_sync(int (*fn)(const int, const void*), const int min,
                 const int max, const void *args);


/// Perform a parallel call.
///
/// This encapsulates a simple parallel for loop with the following semantics.
///
/// @code
/// for (int i = min, e = max; i < e; ++i) {
///   char args[arg_size];
///   arg_init(args, i, env);
///   hpx_call(HPX_HERE, action, arg_size, args, sync);
/// }
/// @endcode
///
/// The loop may actually be spawned as a tree, in which case @p
/// branching_factor controls how many chunks each range is partitioned into,
/// and @p cutoff controls the smalled chunk that is split.
///
/// @param           action The action to perform.
/// @param              min The minimum index in the loop.
/// @param              max The maximum index in the loop.
/// @param branching_factor The tree branching factor for tree-spawn.
/// @param           cutoff The largest chunk we won't split.
/// @param         arg_size The size of the arguments to action.
/// @param         arg_init A callback to initialize the arguments
/// @param         env_size The size of the environment of arg_init.
/// @param              env An environment to pass to arg_init.
/// @param             sync An LCO to set as the continuation for each iteration.
///
//// @returns An error code, or HPX_SUCCESS.
int hpx_par_call(hpx_action_t action,
                const int min, const int max,
                const int branching_factor, const int cutoff,
                const size_t arg_size,
                void (*arg_init)(void*, const int, const void*),
                const size_t env_size, const void *env,
                hpx_addr_t sync);


int hpx_par_call_sync(hpx_action_t action,
                     const int min, const int max,
                     const int branching_factor, const int cutoff,
                     const size_t arg_size,
                     void (*arg_init)(void*, const int, const void*),
                     const size_t env_size,
                     const void *env);

#endif // HPX_PAR_H
