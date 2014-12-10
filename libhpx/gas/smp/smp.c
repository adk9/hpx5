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

#include <stdbool.h>
#include <string.h>
#include <jemalloc/jemalloc.h>
#include "libhpx/gas.h"
#include "libhpx/libhpx.h"
#include "libhpx/locality.h"

static int _smp_join(void) {
  return LIBHPX_OK;
}

static void _smp_leave(void) {
}

static void _smp_delete(gas_t *gas) {
}

static bool _smp_is_global(gas_t *gas, void *addr) {
  return true;
}

static uint32_t _smp_locality_of(hpx_addr_t addr) {
  return 0;
}

static int64_t _smp_sub(hpx_addr_t lhs, hpx_addr_t rhs, uint32_t bsize) {
  return (lhs - rhs);
}

static hpx_addr_t _smp_add(hpx_addr_t gva, int64_t bytes, uint32_t bsize) {
  hpx_addr_t addr = gva + bytes;
  return addr;
}

static hpx_addr_t _smp_lva_to_gva(void *lva) {
  hpx_addr_t addr = (hpx_addr_t)lva;
  return addr;
}

static void *_smp_gva_to_lva(hpx_addr_t addr) {
  return (void*)addr;
}

static bool _smp_try_pin(const hpx_addr_t addr, void **local) {
  if (local)
    *local = _smp_gva_to_lva(addr);
  return true;
}

static void _smp_unpin(const hpx_addr_t addr) {
}

static hpx_addr_t _smp_there(uint32_t i) {
  return _smp_lva_to_gva(0);
}

static hpx_addr_t _smp_gas_cyclic_alloc(size_t n, uint32_t bsize) {
  return _smp_lva_to_gva(libhpx_global_malloc(n * bsize));
}

static hpx_addr_t _smp_gas_cyclic_calloc(size_t n, uint32_t bsize) {
  return _smp_lva_to_gva(libhpx_global_calloc(n, bsize));
}

static hpx_addr_t _smp_gas_alloc(uint32_t bytes) {
  return _smp_lva_to_gva(libhpx_global_malloc(bytes));
}

static void _smp_gas_free(hpx_addr_t addr, hpx_addr_t sync) {
  libhpx_global_free(_smp_gva_to_lva(addr));
  if (sync)
    hpx_lco_set(sync, 0, NULL, HPX_NULL, HPX_NULL);
}

static int _smp_memcpy(hpx_addr_t to, hpx_addr_t from, size_t size,
                       hpx_addr_t sync) {
  if (!size)
    return HPX_SUCCESS;

  void *lto = _smp_gva_to_lva(to);
  const void *lfrom = _smp_gva_to_lva(from);
  memcpy(lto, lfrom, size);

  if (sync)
    hpx_lco_set(sync, 0, NULL, HPX_NULL, HPX_NULL);

  return HPX_SUCCESS;
}

static int _smp_memput(hpx_addr_t to, const void *from, size_t size,
                       hpx_addr_t lsync, hpx_addr_t rsync) {
  if (!size)
    return HPX_SUCCESS;

  void *lto = _smp_gva_to_lva(to);
  memcpy(lto, from, size);

  if (lsync)
    hpx_lco_set(lsync, 0, NULL, HPX_NULL, HPX_NULL);

  if (rsync)
    hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);

  return HPX_SUCCESS;
}

static int _smp_memget(void *to, hpx_addr_t from, size_t size, hpx_addr_t lsync)
{
  if (!size)
    return HPX_SUCCESS;

  const void *lfrom = _smp_gva_to_lva(from);
  memcpy(to, lfrom, size);

  if (lsync)
    hpx_lco_set(lsync, 0, NULL, HPX_NULL, HPX_NULL);

  return HPX_SUCCESS;
}

static void _smp_move(hpx_addr_t src, hpx_addr_t dst, hpx_addr_t sync) {
  if (sync)
    hpx_lco_set(sync, 0, NULL, HPX_NULL, HPX_NULL);
}

static uint32_t _smp_owner_of(hpx_addr_t addr) {
  return 0;
}

static gas_t _smp_vtable = {
  .type          = HPX_GAS_SMP,
  .delete        = _smp_delete,
  .join          = _smp_join,
  .leave         = _smp_leave,
  .is_global     = _smp_is_global,
  .locality_of   = _smp_locality_of,
  .sub           = _smp_sub,
  .add           = _smp_add,
  .lva_to_gva    = _smp_lva_to_gva,
  .gva_to_lva    = _smp_gva_to_lva,
  .there         = _smp_there,
  .try_pin       = _smp_try_pin,
  .unpin         = _smp_unpin,
  .cyclic_alloc  = _smp_gas_cyclic_alloc,
  .cyclic_calloc = _smp_gas_cyclic_calloc,
  .local_alloc   = _smp_gas_alloc,
  .free          = _smp_gas_free,
  .move          = _smp_move,
  .memget        = _smp_memget,
  .memput        = _smp_memput,
  .memcpy        = _smp_memcpy,
  .owner_of      = _smp_owner_of
};

gas_t *gas_smp_new(void) {
  return &_smp_vtable;
}
