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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/// @file libhpx/gas/parcel_memput.c
/// @brief Implements hpx_gas_memput() using two-sided parcels
///

#include <string.h>
#include <hpx/hpx.h>
#include "libhpx/action.h"
#include "libhpx/debug.h"
#include "emulation.h"

static hpx_action_t _memput_request = 0;

static int _memput_request_action(void *args) {
  hpx_addr_t target = hpx_thread_current_target();
  char *local;
  if (!hpx_gas_try_pin(target, (void**)&local))
    return HPX_RESEND;

  memcpy(local, args, hpx_thread_current_args_size());
  hpx_gas_unpin(target);
  return HPX_SUCCESS;
}

static HPX_CONSTRUCTOR void _init_actions(void) {
  LIBHPX_REGISTER_ACTION(_memput_request_action, &_memput_request);
}

int parcel_memput(hpx_addr_t to, const void *from, size_t size,
                  hpx_addr_t lsync, hpx_addr_t rsync) {
  int e = hpx_call_async(to, _memput_request, from, size, lsync, rsync);
  dbg_check(e, "failed to initiate a memput request.\n");
  return e;
}
