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
#ifndef LIBHPX_ACTION_H
#define LIBHPX_ACTION_H

#include "hpx/hpx.h"

HPX_INTERNAL hpx_action_t action_register(const char * key,
                                          hpx_action_handler_t f)
  HPX_NON_NULL(1);

HPX_INTERNAL hpx_action_handler_t action_lookup(hpx_action_t id);

HPX_INTERNAL int action_invoke(hpx_action_t id, void *args);

#endif // LIBHPX_ACTION_H
