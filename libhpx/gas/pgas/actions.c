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

#include <string.h>
#include "libhpx/locality.h"
#include "gva.h"
#include "heap.h"
#include "pgas.h"

hpx_action_t pgas_cyclic_alloc = 0;
hpx_action_t pgas_cyclic_calloc = 0;
hpx_action_t pgas_memset = 0;

hpx_addr_t pgas_cyclic_alloc_sync(size_t n, uint32_t bsize) {
  const uint64_t bpn = pgas_n_per_locality(n, here->ranks);
  const uint32_t balign = pgas_fit_log2_32(bsize);
  const size_t goffset = heap_csbrk(global_heap, bpn, balign);
  const pgas_gva_t gva = pgas_gva_from_goffset(here->rank, goffset, here->ranks);
  const hpx_addr_t addr = HPX_ADDR_INIT(gva, 0, 0);
  return addr;
}

hpx_addr_t pgas_cyclic_calloc_sync(size_t n, uint32_t bsize) {
  const uint64_t bpn = pgas_n_per_locality(n, here->ranks);
  const uint32_t balign = pgas_fit_log2_32(bsize);
  const size_t goffset = heap_csbrk(global_heap, bpn, balign);

  pgas_memset_args_t args = {
    .goffset = goffset,
    .value = 0,
    .length = bpn * balign
  };

  hpx_addr_t sync = hpx_lco_future_new(0);
  hpx_bcast(pgas_memset, &args, sizeof(args), sync);
  const pgas_gva_t gva = pgas_gva_from_goffset(here->rank, goffset, here->ranks);
  const hpx_addr_t addr = HPX_ADDR_INIT(gva, 0, 0);
  hpx_lco_wait(sync);
  hpx_lco_delete(sync, HPX_NULL);
  return addr;
}

static int _cyclic_alloc_async(pgas_alloc_args_t *args) {
  hpx_addr_t addr = pgas_cyclic_alloc_sync(args->n, args->bsize);
  HPX_THREAD_CONTINUE(addr);
}

static int _cyclic_calloc_async(pgas_alloc_args_t *args) {
  hpx_addr_t addr = pgas_cyclic_calloc_sync(args->n, args->bsize);
  HPX_THREAD_CONTINUE(addr);
}

static int _pgas_memset_async(pgas_memset_args_t *args) {
  void *dest = heap_offset_to_local(global_heap, args->goffset);
  memset(dest, args->value, args->length);
  return HPX_SUCCESS;
}

void pgas_register_actions(void) {
  pgas_cyclic_alloc = HPX_REGISTER_ACTION(_cyclic_alloc_async);
  pgas_cyclic_calloc = HPX_REGISTER_ACTION(_cyclic_calloc_async);
  pgas_memset = HPX_REGISTER_ACTION(_pgas_memset_async);
}

static void HPX_CONSTRUCTOR _register(void) {
  pgas_register_actions();
}
