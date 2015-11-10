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

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <hpx/hpx.h>
#include <libhpx/action.h>
#include <libhpx/config.h>
#include <libhpx/debug.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/parcel.h>
#include <libhpx/percolation.h>
#include <libhpx/scheduler.h>


percolation_t *percolation_new(void) {
#ifdef HAVE_OPENCL
  return percolation_new_opencl();
#endif
  log_error("percolation: no usable back-end found!\n");
  return NULL;
}

int percolation_execute_handler(int nargs, void *vargs[],
                                size_t sizes[]) {
  hpx_parcel_t *p = scheduler_current_parcel();

  percolation_t *percolation = here->percolation;
  dbg_assert(percolation);

  const struct action_table *actions = here->actions;
  dbg_assert(actions);

  void *obj = action_table_get_env(actions, p->action);
  if (!obj) {
    return HPX_ERROR;
  }

  int e = percolation_execute(percolation, obj, nargs, vargs, sizes);
  return e;
}
LIBHPX_ACTION(HPX_DEFAULT, HPX_MARSHALLED | HPX_VECTORED,
              percolation_execute_action, percolation_execute_handler,
              HPX_INT, HPX_POINTER, HPX_POINTER);
