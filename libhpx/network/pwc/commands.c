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

#include <libhpx/debug.h>
#include "commands.h"

static int _release_parcel(int src, command_t command) {
  uintptr_t arg = command_get_arg(command);
  hpx_parcel_t *p = (hpx_parcel_t *)arg;
  log_net("releasing sent parcel %p\n", (void*)p);
  hpx_parcel_release(p);
  return HPX_SUCCESS;
}
HPX_ACTION_DEF(INTERRUPT, _release_parcel, release_parcel, HPX_INT, HPX_UINT64);


static int _recv_parcel(int src, command_t command) {
  return HPX_ERROR;
}
HPX_ACTION_DEF(INTERRUPT, _recv_parcel, recv_parcel, HPX_INT, HPX_UINT64);
