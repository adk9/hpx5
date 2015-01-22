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

/// @file libhpx/gas/parcel_memcpy.c
/// @brief Implements hpx_gas_memcpy() using two-sided parcels
///

#include <string.h>
#include <hpx/hpx.h>
#include "libhpx/action.h"
#include "libhpx/debug.h"
#include "libhpx/parcel.h"
#include "emulation.h"

static hpx_action_t _memcpy_request = 0;
static hpx_action_t _memcpy_reply = 0;

// Just copy the data into the target block.
static int _memcpy_reply_action(void *data) {
  hpx_addr_t target = hpx_thread_current_target();
  char *local;
  if (!hpx_gas_try_pin(target, (void**)&local))
    return HPX_RESEND;

  memcpy(local, data, hpx_thread_current_args_size());
  hpx_gas_unpin(target);
  return HPX_SUCCESS;
}

typedef struct {
  size_t     size;
  hpx_addr_t   to;
  hpx_addr_t sync;
} _memcpy_request_args_t;

static int _memcpy_request_action(_memcpy_request_args_t *args) {
  hpx_addr_t target = hpx_thread_current_target();
  char *local;
  if (!hpx_gas_try_pin(target, (void**)&local))
    return HPX_RESEND;

  int e = hpx_call(args->to, _memcpy_reply, args->sync, local, args->size);
  dbg_check(e, "could not initiate a memcpy reply.\n");
  hpx_gas_unpin(target);
  return e;
}

static HPX_CONSTRUCTOR void _init_actions(void) {
  LIBHPX_REGISTER_ACTION(_memcpy_request_action, &_memcpy_request);
  LIBHPX_REGISTER_ACTION(_memcpy_reply_action, &_memcpy_reply);
}

int parcel_memcpy(hpx_addr_t to, hpx_addr_t from, size_t size, hpx_addr_t sync)
{
  _memcpy_request_args_t args = {
    .size = size,
    .to = to,
    .sync = sync
  };

  int e = hpx_call(from, _memcpy_request, HPX_NULL, &args, sizeof(args));
  dbg_check(e, "Failed to initiate a memcpy request.\n");
  return e;
}

