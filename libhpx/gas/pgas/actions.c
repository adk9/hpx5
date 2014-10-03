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
# include "config.h"
#endif

#include "libhpx/locality.h"
#include "gva.h"
#include "heap.h"
#include "pgas.h"

hpx_action_t act_pgas_cyclic_alloc_handler = 0;
hpx_action_t act_pgas_cyclic_calloc_handler = 0;

void pgas_register_actions(void) {
  act_pgas_cyclic_alloc_handler = HPX_REGISTER_ACTION(pgas_cyclic_alloc_handler);
  act_pgas_cyclic_calloc_handler = HPX_REGISTER_ACTION(pgas_cyclic_calloc_handler);
}

int pgas_cyclic_alloc_handler(alloc_handler_args_t *args) {
  const size_t goffset = heap_sbrk(global_heap, args->n, args->bsize);
  pgas_gva_t gva = pgas_gva_from_goffset(here->rank, goffset, here->ranks);
  hpx_addr_t addr = HPX_ADDR_INIT(gva, 0, 0);
  HPX_THREAD_CONTINUE(addr);
}

int pgas_cyclic_calloc_handler(alloc_handler_args_t *args) {
  const size_t goffset = heap_sbrk(global_heap, args->n, args->bsize);
  pgas_gva_t gva = pgas_gva_from_goffset(here->rank, goffset, here->ranks);
  hpx_addr_t addr = HPX_ADDR_INIT(gva, 0, 0);
  HPX_THREAD_CONTINUE(addr);
}


