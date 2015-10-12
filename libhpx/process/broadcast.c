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

#include <hpx/hpx.h>

int _hpx_process_broadcast(hpx_pid_t pid, hpx_action_t action, hpx_addr_t lsync,
                           hpx_addr_t rsync, int nargs, ...) {
  return HPX_SUCCESS;
}

int _hpx_process_broadcast_lsync(hpx_pid_t pid, hpx_action_t action,
                                 hpx_addr_t rsync, int nargs, ...) {
  return HPX_SUCCESS;
}

int _hpx_process_broadcast_rsync(hpx_pid_t pid, hpx_action_t action, int nargs,
                                 ...) {
  return HPX_SUCCESS;
}
