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

#ifndef HPX_PROCESS_H
#define HPX_PROCESS_H

/// @defgroup processes Processes
/// @brief Functions and definitions for using HPX processes
/// @{

typedef hpx_addr_t hpx_pid_t;

/// HPX Process creation.
///
/// This function calls the specified @p action with the @p args and @
/// len in a new process context. Processes in HPX are part of a
/// termination group and can be waited on through the @p termination
/// LCO. The returned @p process object uniquely represents a process
/// and permits operations to be executed on the process.
///
/// NB: a process spawn is always local to the calling locality.
hpx_addr_t hpx_process_new(hpx_addr_t termination) HPX_PUBLIC;


/// A process-specific call interface.
///
/// This calls an action @p action inside a process @p process putting
/// the resulting value in @p result at some point in the future.
int    _hpx_process_call(hpx_addr_t process, hpx_addr_t addr, hpx_action_t action,
                         hpx_addr_t result, int nargs, ...) HPX_PUBLIC;
#define hpx_process_call(process, addr, action, result, ...)                \
  _hpx_process_call(process, addr, action, result, __HPX_NARGS(__VA_ARGS__),\
                    __VA_ARGS__)

void hpx_process_delete(hpx_addr_t process, hpx_addr_t sync) HPX_PUBLIC;

hpx_pid_t hpx_process_getpid(hpx_addr_t process) HPX_PUBLIC;

/// Allocate a distributed allreduce collective in the current process.
///
/// This allreduce has basically the same behavior as a traditional allreduce
/// collective in a SPMD model, except that there is no expectation that the
/// inputs to the allreduce are perfectly balanced.
///
/// This allreduce guarantees a deterministic reduce order, so floating point
/// reductions should be deterministic. It does not, however, specify any
/// specific execution tree, so the error may vary due to machine precision.
///
/// @param        bytes The size, in bytes, of the reduced value.
/// @param       inputs The number of inputs to the allreduce.
/// @param           op The reduce operation.
///
/// @returns            The global address to use for the allreduce, or HPX_NULL
///                     if there was an allocation problem.
hpx_addr_t hpx_process_collective_allreduce_new(size_t bytes, int inputs,
                                                hpx_action_t op)
  HPX_PUBLIC;

/// Delete a process allreduce.
///
/// This is not synchronized, so the caller must ensure that there are no
/// concurrent accesses to the allreduce.
///
/// @param    allreduce The collective's address.
void hpx_process_collective_allreduce_delete(hpx_addr_t allreduce)
  HPX_PUBLIC;


/// Join an allreduce asynchronously.
///
/// This interface joins an allreduce, and provides a continuation for the
/// allreduce value once it is computed. This will often be an LCO set
/// operation.
///
/// @param    allreduce The collective address.
/// @param           id The input id.
/// @param        bytes The size of the allreduce value in bytes.
/// @param           in A pointer to the allreduce input value.
/// @param     c_action The continuation action for the allreduce result.
/// @param     c_target The continuation target for the allreduce result.
///
/// @return             HPX_SUCCESS if the local part of this operation
///                     completes successfully, or an error code if it fails.
int hpx_process_collective_allreduce_join(hpx_addr_t allreduce,
                                          int id, size_t bytes, const void *in,
                                          hpx_action_t c_action,
                                          hpx_addr_t c_target)
  HPX_PUBLIC;

/// Join an allreduce synchronously.
///
/// This interface joins an allreduce, and will block the calling thread until
/// the reduced value is available in @p out. Performance may be improved if the
/// @p out buffer should be from registered memory (i.e., a stack location, a
/// pinned global address, or memory returned from malloc_registered()).
///
/// @param    allreduce The collective address.
/// @param           id The input id.
/// @param        bytes The size of the allreduce value in bytes.
/// @param           in A pointer to the allreduce input value.
/// @param          out A pointer to at least @p bytes for the reduced value,
///                     may alias @p in.
///
/// @return             HPX_SUCCESS if the local part of this operation
///                     completes successfully, or an error code if it fails.
int hpx_process_collective_allreduce_join_sync(hpx_addr_t collective, int id,
                                               size_t bytes, const void *in,
                                               void *out)
  HPX_PUBLIC;

/// @}

#endif
