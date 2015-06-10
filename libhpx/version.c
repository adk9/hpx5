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

#include <stdio.h>
#include <hpx/hpx.h>
#include <libhpx/libhpx.h>

void hpx_print_version(void) {
  printf("HPX version %s\n", HPX_VERSION);
}

void libhpx_print_version(void) {
  printf("libhpx version %s\n", LIBHPX_VERSION);
}