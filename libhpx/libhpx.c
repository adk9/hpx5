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
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>


libhpx_config_t *libhpx_get_config(void) {
  dbg_assert_str(here, "libhpx not initialized\n");
  dbg_assert_str(here->config, "libhpx config not available yet\n");
  return here->config;
}
