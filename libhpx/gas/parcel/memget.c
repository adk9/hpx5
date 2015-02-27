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

/// @file libhpx/gas/parcel_memget.c
/// @brief Implements hpx_gas_memget() using two-sided parcels
///

#include <string.h>
#include <hpx/hpx.h>
#include "libhpx/action.h"
#include "libhpx/debug.h"
#include "libhpx/parcel.h"
#include "emulation.h"

typedef struct {
  void    *to;
  char from[];
} _memget_reply_args_t;

static HPX_ACTION(_memget_reply, _memget_reply_args_t *args) {
  memcpy(args->to, &args->from,
         hpx_thread_current_args_size() - sizeof(args->to));
  return HPX_SUCCESS;
}


typedef struct {
  size_t      size;
  void         *to;
  hpx_addr_t lsync;
  hpx_addr_t reply;
} _memget_request_args_t;


static HPX_ACTION(_memget_request, _memget_request_args_t *args) {
  // not using pinned action because I have an early unpin below
  hpx_addr_t target = hpx_thread_current_target();
  char *local;
  if (!hpx_gas_try_pin(target, (void**)&local))
    return HPX_RESEND;

  hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(_memget_reply_args_t) +
                                       args->size);
  assert(p);

  hpx_parcel_set_action(p, _memget_reply);
  hpx_parcel_set_target(p, args->reply);
  hpx_parcel_set_cont_action(p, hpx_lco_set_action);
  hpx_parcel_set_cont_target(p, args->lsync);

  _memget_reply_args_t *repargs = hpx_parcel_get_data(p);
  repargs->to = args->to;
  memcpy(&repargs->from, local, args->size);
  hpx_gas_unpin(target);
  hpx_parcel_send_sync(p);
  return HPX_SUCCESS;
}

int parcel_memget(void *to, hpx_addr_t from, size_t size, hpx_addr_t lsync) {
  _memget_request_args_t args = {
    .size = size,
    .to = to,
    .lsync = lsync,
    .reply = HPX_HERE
  };

  int e = hpx_call(from, _memget_request, HPX_NULL, &args, sizeof(args));
  dbg_check(e, "Failed to initiate a memget request.\n");
  return e;
}

