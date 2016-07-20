// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2016, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================

#ifndef LIBHPX_NETWORK_INST_H
#define LIBHPX_NETWORK_INST_H

#include <hpx/attributes.h>

#ifdef __cplusplus
extern "C" {
#endif

void *network_inst_new(void *impl)
  HPX_MALLOC;

#ifdef __cplusplus
}
#endif

#endif
