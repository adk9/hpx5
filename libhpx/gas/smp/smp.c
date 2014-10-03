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
#include <jemalloc/jemalloc.h>
#include "libhpx/gas.h"
#include "libhpx/libhpx.h"

static int _smp_join(void) {
  return LIBHPX_OK;
}

static void _smp_leave(void) {
}

static void _smp_delete(gas_class_t *gas) {
}

static bool _smp_is_global(gas_class_t *gas, void *addr) {
  return true;
}

static uint32_t _smp_locality_of(hpx_addr_t addr) {
  return 0;
}

static uint64_t _smp_offset_of(hpx_addr_t gva, uint32_t bsize) {
  return gva.offset;
}

static uint32_t _smp_phase_of(hpx_addr_t gva, uint32_t bsize) {
  return 0;
}

static int64_t _smp_sub(hpx_addr_t lhs, hpx_addr_t rhs, uint32_t bsize) {
  return (lhs.offset - rhs.offset);
}

static hpx_addr_t _smp_add(hpx_addr_t gva, int64_t bytes, uint32_t bsize) {
  hpx_addr_t addr = {
    .offset = gva.offset + bytes,
    .base_id = 0,
    .block_bytes = 0
  };
  return addr;
}

static gas_class_t _smp_vtable = {
  .type   = HPX_GAS_SMP,
  .delete = _smp_delete,
  .join   = _smp_join,
  .leave  = _smp_leave,
  .is_global = _smp_is_global,
  .global = {
    .malloc         = libhpx_malloc,
    .free           = libhpx_free,
    .calloc         = libhpx_calloc,
    .realloc        = libhpx_realloc,
    .valloc         = libhpx_valloc,
    .memalign       = libhpx_memalign,
    .posix_memalign = libhpx_posix_memalign
  },
  .local  = {
    .malloc         = libhpx_malloc,
    .free           = libhpx_free,
    .calloc         = libhpx_calloc,
    .realloc        = libhpx_realloc,
    .valloc         = libhpx_valloc,
    .memalign       = libhpx_memalign,
    .posix_memalign = libhpx_posix_memalign
  },
  .locality_of = _smp_locality_of,
  .offset_of = _smp_offset_of,
  .phase_of = _smp_phase_of,
  .sub = _smp_sub,
  .add = _smp_add
};

gas_class_t *gas_smp_new(size_t heap_size, struct boot_class *boot,
                         struct transport_class *transport) {
  return &_smp_vtable;
}
