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

void entry_init_handlers(action_entry_t *entry) {
  if (entry_is_vectored(entry)) {
    entry_init_vectored(entry);
  }
  else if (entry_is_marshalled(entry)) {
    entry_init_marshalled(entry);
  }
  else if (entry_is_ffi(entry)) {
    entry_init_ffi(entry);
  }
  else {
    dbg_error("Could not initialize entry for attr %" PRIu32 "\n", entry->attr);
  }
}
