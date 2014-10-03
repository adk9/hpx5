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
#ifndef LIBHPX_GAS_PGAS_H
#define LIBHPX_GAS_PGAS_H

#include <hpx/hpx.h>
#include <hpx/attributes.h>

extern struct heap *global_heap;

typedef struct {
  size_t n;
  uint32_t bsize;
} alloc_handler_args_t;

int pgas_cyclic_alloc_handler(alloc_handler_args_t *args)
  HPX_INTERNAL;

extern hpx_action_t act_pgas_cyclic_alloc_handler;

int pgas_cyclic_calloc_handler(alloc_handler_args_t *args)
  HPX_INTERNAL;

extern hpx_action_t act_pgas_cyclic_calloc_handler;

void pgas_register_actions(void)
  HPX_INTERNAL;

#endif // LIBHPX_GAS_PGAS_H
