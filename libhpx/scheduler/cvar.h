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
#ifndef LIBHPX_SCHEDULER_CVAR_H
#define LIBHPX_SCHEDULER_CVAR_H

#include "libhpx/scheduler.h"

typedef struct cvar_node {
  struct cvar_node *next;
  hpx_parcel_t     *data;
  uint32_t           tid;
} cvar_node_t;

typedef struct cvar {
  cvar_node_t *top;
  hpx_status_t status;
} cvar_t;


#endif // LIBHPX_SCHEDULER_CVAR_H
