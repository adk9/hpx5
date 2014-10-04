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


#include <hpx/hpx.h>
#include <libhpx/gas.h>
#include <libhpx/locality.h>

const hpx_addr_t HPX_NULL = HPX_ADDR_INIT(0, 0, 0);

hpx_addr_t HPX_HERE = HPX_ADDR_INIT(0, 0, 0);

hpx_addr_t HPX_THERE(hpx_locality_t i) {
  assert(here && here->gas && here->gas->there);
  return here->gas->there(i);
}

hpx_addr_t hpx_addr_init(uint64_t offset, uint32_t base, uint32_t bytes) {
  const hpx_addr_t addr = HPX_ADDR_INIT(offset, 0, 0);
  return addr;
}

bool hpx_addr_eq(const hpx_addr_t lhs, const hpx_addr_t rhs) {
  return (lhs.offset == rhs.offset) && (lhs.base_id == rhs.base_id) &&
  (lhs.block_bytes == rhs.block_bytes);
}

hpx_addr_t hpx_addr_add(const hpx_addr_t addr, int bytes) {
  assert(here && here->gas);
  return here->gas->add(addr, bytes, addr.block_bytes);
}

bool hpx_gas_try_pin(const hpx_addr_t addr, void **local) {
  assert(here && here->gas);
  return here->gas->try_pin(addr, local);
}

void hpx_gas_unpin(const hpx_addr_t addr) {
  assert(here && here->gas);
  here->gas->unpin(addr);
}

hpx_addr_t hpx_gas_global_alloc(size_t n, uint32_t bsize) {
  assert(here && here->gas);
  return here->gas->cyclic_alloc(n, bsize);
}

hpx_addr_t hpx_gas_global_calloc(size_t n, uint32_t bsize) {
  assert(here && here->gas);
  return here->gas->cyclic_calloc(n, bsize);
}

hpx_addr_t hpx_gas_alloc(uint32_t bsize) {
  assert(here && here->gas);
  return here->gas->local_alloc(bsize);
}

void hpx_gas_free(hpx_addr_t addr, hpx_addr_t sync) {
  assert(here && here->gas);
  here->gas->free(addr, sync);
}

void hpx_gas_move(hpx_addr_t src, hpx_addr_t dst, hpx_addr_t lco) {
  assert(here && here->gas && here->gas->move);
  here->gas->move(src, dst, lco);
}

int hpx_gas_memget(void *to, hpx_addr_t from, size_t size, hpx_addr_t lsync) {
  assert(here && here->gas && here->gas->memget);
  return here->gas->memget(to, from, size, lsync);
}

int hpx_gas_memput(hpx_addr_t to, const void *from, size_t size,
                   hpx_addr_t lsync, hpx_addr_t rsync) {
  assert(here && here->gas && here->gas->memput);
  return here->gas->memput(to, from, size, lsync, rsync);
}

int hpx_gas_memcpy(hpx_addr_t to, hpx_addr_t from, size_t size, hpx_addr_t sync)
{
  assert(here && here->gas && here->gas->memcpy);
  return here->gas->memcpy(to, from, size, sync);
}
