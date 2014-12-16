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
#ifndef LIBHPX_NETWORK_ISIR_EMULATE_PWC_H
#define LIBHPX_NETWORK_ISIR_EMULATE_PWC_H

#include <hpx/hpx.h>

extern hpx_action_t isir_emulate_pwc;

struct isir_emulate_gwc_args {
  size_t            n;
  hpx_addr_t       to;
  hpx_addr_t complete;
};

extern hpx_action_t isir_emulate_gwc;


#endif
