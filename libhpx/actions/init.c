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

#include <inttypes.h>
#include <libhpx/action.h>
#include <libhpx/debug.h>
#include "init.h"

void action_init_handlers(action_t *action) {
  uint32_t attr = action->attr & (HPX_MARSHALLED | HPX_VECTORED);
  switch (attr) {
   case (HPX_ATTR_NONE):
    action_init_ffi(action);
    return;
   case (HPX_MARSHALLED):
    action_init_marshalled(action);
    return;
   case (HPX_MARSHALLED | HPX_VECTORED):
    action_init_vectored(action);
    return;
  }
  dbg_error("Could not initialize action for attr %" PRIu32 "\n", attr);
}
