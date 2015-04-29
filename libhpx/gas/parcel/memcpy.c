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

/// @file libhpx/gas/parcel_memcpy.c
/// @brief Implements hpx_gas_memcpy() using two-sided parcels
///

#include <string.h>
#include <hpx/hpx.h>
#include "libhpx/action.h"
#include "libhpx/debug.h"
#include "libhpx/parcel.h"
#include "emulation.h"

static int _memcpy_reply_handler(char *local, size_t bytes, void *data) {
  dbg_assert(bytes);
  memcpy(local, data, bytes);
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, HPX_PINNED | HPX_MARSHALLED, _memcpy_reply,
                  _memcpy_reply_handler, HPX_SIZE_T, HPX_ADDR);

static int _memcpy_request_handler(char *local, size_t size, hpx_addr_t to) {
  hpx_call_cc(to, _memcpy_reply, NULL, NULL, local, size);
}
static HPX_ACTION(HPX_DEFAULT, HPX_PINNED, _memcpy_request, _memcpy_request_handler,
                  HPX_SIZE_T, HPX_ADDR);

int parcel_memcpy(hpx_addr_t to, hpx_addr_t from, size_t size, hpx_addr_t sync) {
  int e = hpx_call(from, _memcpy_request, sync, &size, &to);
  dbg_check(e, "Failed to initiate a memcpy request.\n");
  return e;
}

