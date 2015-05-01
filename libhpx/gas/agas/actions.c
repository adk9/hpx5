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

#include <string.h>
#include <jemalloc/jemalloc_global.h>
#include <hpx/builtins.h>
#include <libhpx/locality.h>
#include <libhpx/action.h>
#include <libhpx/gpa.h>

#include "agas.h"

hpx_addr_t agas_alloc_cyclic_sync(size_t n, uint32_t bsize) {
  return HPX_NULL;
}

static int _alloc_cyclic_handler(size_t n, size_t bsize) {
  hpx_addr_t addr = agas_alloc_cyclic_sync(n, bsize);
  HPX_THREAD_CONTINUE(addr);
}
HPX_ACTION(HPX_DEFAULT, 0, agas_alloc_cyclic, _alloc_cyclic_handler, HPX_SIZE_T,
           HPX_SIZE_T);

hpx_addr_t agas_calloc_cyclic_sync(size_t n, uint32_t bsize) {
  assert(here->rank == 0);
  return HPX_NULL;
}

static int _calloc_cyclic_handler(size_t n, size_t bsize) {
  hpx_addr_t addr = agas_calloc_cyclic_sync(n, bsize);
  HPX_THREAD_CONTINUE(addr);
}
HPX_ACTION(HPX_DEFAULT, 0, agas_calloc_cyclic, _calloc_cyclic_handler, HPX_SIZE_T,
           HPX_SIZE_T);

