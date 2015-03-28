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


#include <hpx/hpx.h>
#include <libhpx/gas.h>
#include <libhpx/locality.h>

hpx_addr_t HPX_HERE = 0;

hpx_addr_t HPX_THERE(uint32_t i) {
  dbg_assert(here && here->gas && here->gas->there);
  return here->gas->there(i);
}

hpx_addr_t hpx_addr_add(hpx_addr_t addr, int64_t bytes, uint32_t bsize) {
  dbg_assert(here && here->gas && here->gas->add);
  return here->gas->add(addr, bytes, bsize);
}

int64_t hpx_addr_sub(hpx_addr_t lhs, hpx_addr_t rhs, uint32_t bsize) {
  dbg_assert(here && here->gas && here->gas->sub);
  return here->gas->sub(lhs, rhs, bsize);
}

bool hpx_gas_try_pin(const hpx_addr_t addr, void **local) {
  dbg_assert(here && here->gas);
  return here->gas->try_pin(addr, local);
}

void hpx_gas_unpin(const hpx_addr_t addr) {
  dbg_assert(here && here->gas);
  here->gas->unpin(addr);
}

hpx_addr_t hpx_gas_alloc_cyclic(size_t n, uint32_t bsize) {
  dbg_assert(here && here->gas && here->gas->alloc_cyclic);
  return here->gas->alloc_cyclic(n, bsize);
}

hpx_addr_t hpx_gas_calloc_cyclic(size_t n, uint32_t bsize) {
  dbg_assert(here && here->gas && here->gas->calloc_cyclic);
  return here->gas->calloc_cyclic(n, bsize);
}

hpx_addr_t hpx_gas_alloc_blocked(size_t n, uint32_t bsize) {
  dbg_assert(here && here->gas && here->gas->alloc_blocked);
  return here->gas->alloc_blocked(n, bsize);
}

hpx_addr_t hpx_gas_calloc_blocked(size_t n, uint32_t bsize) {
  dbg_assert(here && here->gas && here->gas->calloc_blocked);
  return here->gas->calloc_blocked(n, bsize);
}

hpx_addr_t hpx_gas_alloc_local(uint32_t bsize) {
  dbg_assert(here && here->gas && here->gas->alloc_local);
  return here->gas->alloc_local(bsize);
}

hpx_addr_t hpx_gas_calloc_local(size_t nmemb, size_t size) {
  dbg_assert(here && here->gas && here->gas->calloc_local);
  return here->gas->calloc_local(nmemb, size);
}

void hpx_gas_free(hpx_addr_t addr, hpx_addr_t sync) {
  dbg_assert(here && here->gas);
  here->gas->free(addr, sync);
}

void hpx_gas_move(hpx_addr_t src, hpx_addr_t dst, hpx_addr_t lco) {
  dbg_assert(here && here->gas && here->gas->move);
  here->gas->move(src, dst, lco);
}

int hpx_gas_memget(void *to, hpx_addr_t from, size_t size, hpx_addr_t lsync) {
  dbg_assert(here && here->gas && here->gas->memget);
  return here->gas->memget(to, from, size, lsync);
}

int hpx_gas_memget_sync(void *to, hpx_addr_t from, size_t size) {
  hpx_addr_t lsync = hpx_lco_future_new(0);
  int e;
  e = hpx_gas_memget(to, from, size, lsync);
  dbg_check(e, "failed memget in memget_sync\n");
  e = hpx_lco_wait(lsync);
  hpx_lco_delete(lsync, HPX_NULL);
  return e;
}

int hpx_gas_memput(hpx_addr_t to, const void *from, size_t size,
                   hpx_addr_t lsync, hpx_addr_t rsync) {
  dbg_assert(here && here->gas && here->gas->memput);
  return here->gas->memput(to, from, size, lsync, rsync);
}

int hpx_gas_memcpy(hpx_addr_t to, hpx_addr_t from, size_t size, hpx_addr_t sync)
{
  dbg_assert(here && here->gas && here->gas->memcpy);
  return (*here->gas->memcpy)(to, from, size, sync);
}

static int hpx_gas_alloc_local_at_handler(uint32_t bytes) {
  hpx_addr_t addr = hpx_gas_alloc_local(bytes);
  dbg_assert(addr);
  HPX_THREAD_CONTINUE(addr);
}
HPX_ACTION_DEF(DEFAULT, hpx_gas_alloc_local_at_handler, hpx_gas_alloc_local_at_action,
               HPX_UINT32);

hpx_addr_t hpx_gas_alloc_local_at_sync(uint32_t bytes, hpx_addr_t loc) {
  hpx_addr_t addr = 0;
  int e = hpx_call_sync(loc, hpx_gas_alloc_local_at_action, &addr, sizeof(addr),
                        &bytes);
  dbg_check(e, "Failed synchronous call during allocation\n");
  dbg_assert(addr);
  return addr;
}

void hpx_gas_alloc_local_at_async(uint32_t bytes, hpx_addr_t loc, hpx_addr_t lco) {
  int e = hpx_call(loc, hpx_gas_alloc_local_at_action, lco, &bytes);
  dbg_check(e, "Failed async call during allocation\n");
}

static int _gas_calloc_at_handler(size_t nmemb, size_t size) {
  hpx_addr_t addr = hpx_gas_calloc_local(nmemb, size);
  dbg_assert(addr);
  HPX_THREAD_CONTINUE(addr);
}
HPX_ACTION_DEF(DEFAULT, _gas_calloc_at_handler, hpx_gas_calloc_local_at_action,
               HPX_UINT64, HPX_UINT64);

hpx_addr_t hpx_gas_calloc_local_at_sync(size_t nmemb, size_t size, hpx_addr_t loc) {
  hpx_addr_t addr = 0;
  int e = hpx_call_sync(loc, hpx_gas_calloc_local_at_action, &addr, sizeof(addr),
                        &nmemb, &size);
  dbg_check(e, "Failed synchronous call during allocation\n");
  dbg_assert(addr);
  return addr;
}

void hpx_gas_calloc_local_at_async(size_t nmemb, size_t size, hpx_addr_t loc,
                             hpx_addr_t lco) {
  int e = hpx_call(loc, hpx_gas_calloc_local_at_action, lco, &nmemb, &size);
  dbg_check(e, "Failed async call during allocation\n");
}


hpx_addr_t hpx_gas_memalign(size_t boundary, size_t size) {
  dbg_assert(here && here->gas && here->gas->local_memalign);
  return here->gas->local_memalign(boundary, size);
}

static int _gas_memalign_at_handler(size_t boundary, size_t size) {
  hpx_addr_t addr = hpx_gas_memalign(boundary, size);
  dbg_assert(addr);
  HPX_THREAD_CONTINUE(addr);
}
HPX_ACTION_DEF(DEFAULT, _gas_memalign_at_handler, hpx_gas_memalign_at_action,
               HPX_UINT64, HPX_UINT64);

hpx_addr_t hpx_gas_memalign_at_sync(size_t boundary, size_t size,
                                    hpx_addr_t loc) {
  hpx_addr_t addr = 0;
  int e = hpx_call_sync(loc, hpx_gas_memalign_at_action, &addr, sizeof(addr),
                        &boundary, &size);
  dbg_check(e, "Failed synchronous call during allocation\n");
  dbg_assert(addr);
  return addr;
}

void hpx_gas_memalign_at_async(size_t boundary, size_t size, hpx_addr_t loc,
                             hpx_addr_t lco) {
  int e = hpx_call(loc, hpx_gas_memalign_at_action, lco, &boundary, &size);
  dbg_check(e, "Failed async call during allocation\n");
}
