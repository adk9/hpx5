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

static HPX_PINNED(_memput_request, void *args) {
  char *local = hpx_thread_current_local_target();
  dbg_assert(local);
  size_t bytes = hpx_thread_current_args_size();
  dbg_assert(bytes);
  memcpy(local, args, bytes);
  return HPX_SUCCESS;
}

int parcel_memput(hpx_addr_t to, const void *from, size_t size,
                  hpx_addr_t lsync, hpx_addr_t rsync) {
  int e = hpx_call_async(to, _memput_request, lsync, rsync, from, size);
  dbg_check(e, "failed to initiate a memput request.\n");
  return e;
}
