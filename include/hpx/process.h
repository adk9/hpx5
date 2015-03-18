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
hpx_addr_t hpx_process_new(hpx_addr_t termination);


/// A process-specific call interface.
///
/// This calls an action @p action inside a process @p process putting
/// the resulting value in @p result at some point in the future.
int    _hpx_process_call(hpx_addr_t process, hpx_addr_t addr, hpx_action_t action,
                         hpx_addr_t result, int nargs, ...);
#define hpx_process_call(process, addr, action, result, ...)                \
  _hpx_process_call(process, addr, action, result, __HPX_NARGS(__VA_ARGS__),\
                    __VA_ARGS__)

void hpx_process_delete(hpx_addr_t process, hpx_addr_t sync);

hpx_pid_t hpx_process_getpid(hpx_addr_t process);

#endif
