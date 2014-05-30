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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "libhpx/action.h"

hpx_action_t action_register(const char * key, hpx_action_handler_t f) {
  return (hpx_action_t)f;
}


hpx_action_handler_t action_lookup(hpx_action_t id) {
  return (hpx_action_handler_t)id;
}

int action_invoke(hpx_action_t action, void *args) {
  hpx_action_handler_t handler = action_lookup(action);
  return handler(args);
}
