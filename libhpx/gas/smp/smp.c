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

/// @file  libhpx/gas/smp/smp.c
///
/// @brief This file contains an implementation of the GAS interface for use
///        when no network is available, or when we are running on a single
///        locality. It simply forwards all requests to the system allocator.
#include <libhpx/debug.h>
#include <libhpx/gas.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/memory.h>
#include <libhpx/system.h>

/// Delete the gas instance.
///
/// The SMP GAS instance is global and immutable. We do not need to do anything
/// with it.
static void _smp_delete(gas_t *gas) {
}

/// Figure out how far apart two addresses are.
static int64_t _smp_sub(hpx_addr_t lhs, hpx_addr_t rhs, uint32_t bsize) {
  dbg_assert(lhs != HPX_NULL);
  dbg_assert(rhs != HPX_NULL);
  return (lhs - rhs);
}

/// Adjust an address by an offset.
static hpx_addr_t _smp_add(hpx_addr_t gva, int64_t bytes, uint32_t bsize) {
  dbg_assert(gva != HPX_NULL);
  return gva + bytes;
}

/// Compute the global address for a local address.
static hpx_addr_t _smp_lva_to_gva(const void *lva) {
  return (hpx_addr_t)lva;
}

/// Perform address translation and pin the global address.
static bool _smp_try_pin(const hpx_addr_t addr, void **local) {
  if (local) {
    // Return the local address, if the user wants it.
    *local = (void*)addr;
  }
  // All addresses are local, so we return true.
  return true;
}

/// Release an address translation.
static void _smp_unpin(const hpx_addr_t addr) {
}

/// Compute the locality address.
static hpx_addr_t _smp_there(uint32_t i) {
  dbg_assert_str(i == 0, "Rank %d does not exist in the SMP GAS\n", i);

  // We use the address of the global "here" locality to represent HPX_HERE.
  return _smp_lva_to_gva(here);
}

/// Allocate a global array.
static hpx_addr_t _smp_gas_alloc_cyclic(size_t n, uint32_t bsize,
                                        uint32_t boundary) {
  void *p = boundary ?
      local_memalign(boundary, n * bsize) : local_malloc(n * bsize);
  return _smp_lva_to_gva(p);
}

/// Allocate a 0-filled global array.
static hpx_addr_t _smp_gas_calloc_cyclic(size_t n, uint32_t bsize,
                                         uint32_t boundary) {
  size_t bytes = n * bsize;
  void *p;
  if (boundary) {
    p = local_memalign(boundary, bytes);
    p = memset(p, 0, bytes);
  } else {
    p = local_calloc(n, bsize);
  }
  return _smp_lva_to_gva(p);
}

/// Allocate a bunch of global memory
static hpx_addr_t _smp_gas_alloc_local(uint32_t bytes, uint32_t boundary) {
  void *p = boundary ? local_memalign(boundary, bytes) : local_malloc(bytes);
  return _smp_lva_to_gva(p);
}

/// Allocate a bunch of initialized global memory
static hpx_addr_t _smp_gas_calloc_local(size_t nmemb, size_t size,
                                        uint32_t boundary) {
  size_t bytes = nmemb * size;
  void *p;
  if (boundary) {
    p = local_memalign(boundary, bytes);
    p = memset(p, 0, bytes);
  } else {
    p = local_calloc(nmemb, size);
  }
  return _smp_lva_to_gva(p);
}

/// Free an allocation.
static void _smp_gas_free(hpx_addr_t addr, hpx_addr_t sync) {
  void *p = (void*)addr;
  free(p);

  // Notify the caller that we're done.
  hpx_lco_set(sync, 0, NULL, HPX_NULL, HPX_NULL);
}

/// Perform a memcpy between two global addresses.
static int _smp_memcpy(hpx_addr_t to, hpx_addr_t from, size_t size,
                       hpx_addr_t sync) {
  if (size) {
    dbg_assert(to != HPX_NULL);
    dbg_assert(from != HPX_NULL);

    void *lto = (void*)to;
    const void *lfrom = (void*)from;
    memcpy(lto, lfrom, size);
  }
  hpx_lco_set(sync, 0, NULL, HPX_NULL, HPX_NULL);
  return HPX_SUCCESS;
}

/// Copy memory from a local address to a global address.
static int _smp_memput(hpx_addr_t to, const void *from, size_t size,
                       hpx_addr_t lsync, hpx_addr_t rsync) {
  if (size) {
    dbg_assert(to != HPX_NULL);
    dbg_assert(from != NULL);

    void *lto = (void*)to;
    memcpy(lto, from, size);
  }
  hpx_lco_set(lsync, 0, NULL, HPX_NULL, HPX_NULL);
  hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
  return HPX_SUCCESS;
}

/// Copy memory from a global address to a local address.
static int _smp_memget(void *to, hpx_addr_t from, size_t size, hpx_addr_t lsync)
{
  if (size) {
    dbg_assert(to != NULL);
    dbg_assert(from != HPX_NULL);

    const void *lfrom = (void*)from;
    memcpy(to, lfrom, size);
  }
  hpx_lco_set(lsync, 0, NULL, HPX_NULL, HPX_NULL);
  return HPX_SUCCESS;
}

/// Move memory from one locality to another.
static void _smp_move(hpx_addr_t src, hpx_addr_t dst, hpx_addr_t sync) {
  dbg_assert(src == HPX_HERE);
  dbg_assert(dst == HPX_HERE);
  hpx_lco_set(sync, 0, NULL, HPX_NULL, HPX_NULL);
}

/// Return the size of the global heap stored locally.
static size_t _smp_local_size(gas_t *gas) {
  dbg_error("SMP execution should not call this function\n");
}


static void *_smp_local_base(gas_t *gas) {
  dbg_error("SMP execution should not call this function\n");
}

static gas_t _smp_vtable = {
  .type           = HPX_GAS_SMP,
  .delete         = _smp_delete,
  .local_size     = _smp_local_size,
  .local_base     = _smp_local_base,
  .sub            = _smp_sub,
  .add            = _smp_add,
  .there          = _smp_there,
  .try_pin        = _smp_try_pin,
  .unpin          = _smp_unpin,
  .alloc_cyclic   = _smp_gas_alloc_cyclic,
  .calloc_cyclic  = _smp_gas_calloc_cyclic,
  .alloc_blocked  = NULL,
  .calloc_blocked = NULL,
  .alloc_local    = _smp_gas_alloc_local,
  .calloc_local   = _smp_gas_calloc_local,
  .free           = _smp_gas_free,
  .move           = _smp_move,
  .memget         = _smp_memget,
  .memput         = _smp_memput,
  .memcpy         = _smp_memcpy,
  .mmap           = system_mmap,
  .munmap         = system_munmap
};

gas_t *gas_smp_new(void) {
  return &_smp_vtable;
}
