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

#include <inttypes.h>
#include <limits.h>
#include <string.h>
#include <hpx/hpx.h>
#include "libhpx/boot.h"
#include "libhpx/debug.h"
#include "libhpx/gas.h"
#include "libhpx/libhpx.h"
#include "libhpx/locality.h"
#include "../mallctl.h"
#include "bitmap.h"
#include "gpa.h"
#include "heap.h"
#include "pgas.h"
#include "../parcel/emulation.h"

/// The PGAS type is a global address space that manages a shared heap.
///
/// @note This is sort of silly. The PGAS gas is basically an instance of an
///       object that uses the heap. A more normal approach to this would be to
///       make the heap an instance variable of the PGAS gas subtype. The reason
///       we're currently doing the global_heap as a static is that other files
///       in the pgas module want to interact with the global_heap directly, and
///       we didn't really want to expose the entire PGAS class. For a nicer
///       look, we could adjust this and make everyone in PGAS that needs the
///       heap go through a cast of the locality gas reference, e.g.,
///       (pgas_gas_t*)here->gas->heap.
///
heap_t *global_heap = NULL;

/// Some arena stuff for jemalloc.
static __thread unsigned _global_arena = UINT_MAX;
static __thread unsigned _primordial_arena = UINT_MAX;


/// The static chunk allocator callback that we give to jemalloc arenas that
/// manage our global heap.
///
/// When a jemalloc arena needs to service an allocation request that it does
/// not currently have enough correctly aligned space to deal with, it will use
/// the its currently configured chunk_alloc_t callback to get more space from
/// the system. This is typically done using mmap(), however for memory
/// corresponding to the global address space we want to provide memory from our
/// pre-registered global heap. This callback performs that operation.
///
/// @note This callback is only necessary to pick up the global heap pointer,
///       because the jemalloc callback registration doesn't allow us to
///       register user data to be passed back to us.
///
/// @note I do not know what the @p arena index is useful for---Luke.
///
/// @param[in]   UNUSED1 A requested address for chunk extension.
/// @param[in]      size The number of bytes we need to allocate.
/// @param[in]     align The alignment that is being requested.
/// @param[in/out]  zero Set to zero if the chunk should be zeroed.
/// @param[in]   UNUSED2 The index of the arena making this allocation request.
///
/// @returns The base pointer of the newly allocated chunk.
static void *_chunk_alloc(void *UNUSED1, size_t size, size_t align, bool *zero,
                          unsigned UNUSED2)
{
  void *chunk = heap_chunk_alloc(global_heap, size, align);
  if (zero && *zero)
    memset(chunk, 0, size);
  return chunk;
}


/// The static chunk de-allocator callback that we give to jemalloc arenas that
/// manage our global heap.
///
/// When a jemalloc arena wants to de-allocate a previously-allocated chunk for
/// any reason, it will use its currently configured chunk_dalloc_t callback to
/// do so. This is typically munmap(), however for memory corresponding to the
/// global address space we want to return the memory to our heap. This callback
/// performs that operation.
///
/// @note This callback is only necessary to pick up the global heap pointer,
///       because the jemalloc callback registration doesn't allows us to
///       register user data to be passed back to us.
///
/// @note I do not know what use the @p arena index is---Luke.
///
/// @note I do not know what the return value is used for---Luke.
///
/// @param   chunk The base address of the chunk to de-allocate, must match an
///                address returned from _chunk_alloc().
/// @param    size The number of bytes that were originally requested, must
///                match the number of bytes provided to the _chunk_alloc()
///                request associated with @p chunk.
/// @param   arena The index of the arena making the call to _chunk_dalloc().
///
/// @returns UNKNOWN---Luke.
static bool _chunk_dalloc(void *chunk, size_t size, unsigned UNUSED) {
  return heap_chunk_dalloc(global_heap, chunk, size);
}


int pgas_join(void) {
  if (!global_heap) {
    dbg_error("attempt to join GAS before global heap allocation.\n");
    return LIBHPX_ERROR;
  }

  if (_global_arena == UINT_MAX) {
    _global_arena = mallctl_create_arena(_chunk_alloc, _chunk_dalloc);
    mallctl_thread_enable_cache();
    mallctl_thread_flush_cache();
    _primordial_arena = mallctl_thread_set_arena(_global_arena);
  }
  return LIBHPX_OK;
}


void pgas_leave(void) {
  if (_global_arena == UINT_MAX)
    dbg_error("trying to leave the GAS before joining it.\n");

  mallctl_thread_flush_cache();
  mallctl_thread_set_arena(_primordial_arena);
}


static void _pgas_delete(gas_t *gas) {
  if (global_heap) {
    heap_fini(global_heap);
    free(global_heap);
    global_heap = NULL;
  }
}


static bool _pgas_is_global(gas_t *gas, void *lva) {
  return heap_contains_lva(global_heap, lva);
}


static bool _gpa_is_cyclic(hpx_addr_t gpa) {
  return heap_offset_is_cyclic(global_heap, pgas_gpa_to_offset(gpa));
}


hpx_addr_t pgas_lva_to_gpa(void *lva) {
  const uint64_t offset = heap_lva_to_offset(global_heap, lva);
  return pgas_offset_to_gpa(here->rank, offset);
}


void *pgas_gpa_to_lva(hpx_addr_t gpa) {
   const uint64_t offset = pgas_gpa_to_offset(gpa);
   return heap_offset_to_lva(global_heap, offset);
}

static int64_t _pgas_sub(hpx_addr_t lhs, hpx_addr_t rhs, uint32_t bsize) {
  const bool l = _gpa_is_cyclic(lhs);
  const bool r = _gpa_is_cyclic(rhs);
  DEBUG_IF (l != r) {
    dbg_error("cannot compare addresses between different allocations.\n");
  }

  DEBUG_IF (l ^ r) {
    dbg_error("cannot compare cyclic and non-cyclic addresses.\n");
  }

  return (l && r) ? pgas_gpa_sub_cyclic(lhs, rhs, bsize)
                  : pgas_gpa_sub(lhs, rhs);
}


static hpx_addr_t _pgas_add(hpx_addr_t gpa, int64_t bytes, uint32_t bsize) {
  const bool cyclic = _gpa_is_cyclic(gpa);
  return (cyclic) ? pgas_gpa_add_cyclic(gpa, bytes, bsize)
                  : pgas_gpa_add(gpa, bytes);
}


// Compute a global address for a locality.
static hpx_addr_t _pgas_there(uint32_t i) {
  return pgas_offset_to_gpa(i, 0);
}


/// Pin and translate an hpx address into a local virtual address. PGAS
/// addresses don't get pinned, so we're really only talking about translating
/// the address if its local.
static bool _pgas_try_pin(const hpx_addr_t gpa, void **local) {
  if (pgas_gpa_to_rank(gpa) != here->rank)
    return false;

  if (local)
    *local = pgas_gpa_to_lva(gpa);

  return true;
}

static void _pgas_unpin(const hpx_addr_t addr) {
  DEBUG_IF(!_pgas_try_pin(addr, NULL)) {
    dbg_error("%"PRIu64" is not local to %u\n", addr, here->rank);
  }
}


static hpx_addr_t _pgas_gas_cyclic_alloc(size_t n, uint32_t bsize) {
  if (here->rank == 0)
    return pgas_cyclic_alloc_sync(n, bsize);

  hpx_addr_t addr;
  pgas_alloc_args_t args = {
    .n = n,
    .bsize = bsize
  };
  int e = hpx_call_sync(HPX_THERE(0), pgas_cyclic_alloc, &args, sizeof(args),
                        &addr, sizeof(addr));
  dbg_check(e, "Failed to call pgas_cyclic_alloc_handler.\n");
  return addr;
}

static hpx_addr_t _pgas_gas_cyclic_calloc(size_t n, uint32_t bsize) {
  if (here->rank == 0)
    return pgas_cyclic_calloc_sync(n, bsize);

  hpx_addr_t addr;
  pgas_alloc_args_t args = {
    .n = n,
    .bsize = bsize
  };
  int e = hpx_call_sync(HPX_THERE(0), pgas_cyclic_calloc,
                        &args, sizeof(args), &addr, sizeof(addr));
  dbg_check(e, "Failed to call pgas_cyclic_calloc_handler.\n");
  return addr;
}

/// Allocate a single global block from the global heap, and return it as an
/// hpx_addr_t.
static hpx_addr_t _pgas_gas_alloc(uint32_t bytes) {
  void *lva = libhpx_global_malloc(bytes);
  return pgas_lva_to_gpa(lva);
}

/// Free a global address.
///
/// This global address must either be the base of a cyclic allocation, or a
/// block allocated by _pgas_gas_alloc. At this time, we do not attempt to deal
/// with the cyclic allocations, as they are using a simple csbrk allocator.
static void _pgas_gas_free(hpx_addr_t gpa, hpx_addr_t sync) {
  if (gpa == HPX_NULL) {
    return;
  }

  const uint64_t offset = pgas_gpa_to_offset(gpa);

  DEBUG_IF (true) {
    const void *lva = heap_offset_to_lva(global_heap, offset);
    if (!heap_contains_lva(global_heap, lva))
      dbg_error("attempt to free out of bounds offset %"PRIu64"", offset);
  }

  if (heap_offset_is_cyclic(global_heap, offset)) {
    heap_free_cyclic(global_heap, offset);
  }
  else if (pgas_gpa_to_rank(gpa) == here->rank) {
    libhpx_global_free(pgas_gpa_to_lva(offset));
  }
  else {
    int e = hpx_call(gpa, pgas_free, NULL, 0, sync);
    dbg_check(e, "failed to call pgas_free on %"PRIu64"", gpa);
    return;
  }

  if (sync != HPX_NULL)
    hpx_lco_set(sync, 0, NULL, HPX_NULL, HPX_NULL);
}

static int _pgas_parcel_memcpy(hpx_addr_t to, hpx_addr_t from, size_t size,
                               hpx_addr_t sync) {
  if (!size)
    return HPX_SUCCESS;

  const uint32_t rank = here->rank;
  if (pgas_gpa_to_rank(to) == rank && pgas_gpa_to_rank(from) == rank) {
    void *lto = pgas_gpa_to_lva(to);
    const void *lfrom = pgas_gpa_to_lva(from);
    memcpy(lto, lfrom, size);
  }
  else {
    return parcel_memcpy(to, from, size, sync);
  }

  if (sync)
    hpx_lco_set(sync, 0, NULL, HPX_NULL, HPX_NULL);

  return HPX_SUCCESS;
}

static int _pgas_parcel_memput(hpx_addr_t to, const void *from, size_t size,
                               hpx_addr_t lsync, hpx_addr_t rsync) {
  if (!size)
    return HPX_SUCCESS;

  if (pgas_gpa_to_rank(to) == here->rank) {
    void *lto = pgas_gpa_to_lva(to);
    memcpy(lto, from, size);
  }
  else {
    return parcel_memput(to, from, size, lsync, rsync);
  }

  if (lsync)
    hpx_lco_set(lsync, 0, NULL, HPX_NULL, HPX_NULL);
  if (rsync)
    hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
  return HPX_SUCCESS;
}

static int _pgas_parcel_memget(void *to, hpx_addr_t from, size_t size,
                               hpx_addr_t lsync) {
  if (!size)
    return HPX_SUCCESS;

  if (pgas_gpa_to_rank(from) == here->rank) {
    const void *lfrom = pgas_gpa_to_lva(from);
    memcpy(to, lfrom, size);
  }
  else {
    return parcel_memget(to, from, size, lsync);
  }

  if (lsync)
    hpx_lco_set(lsync, 0, NULL, HPX_NULL, HPX_NULL);
  return HPX_SUCCESS;
}

static void _pgas_move(hpx_addr_t src, hpx_addr_t dst, hpx_addr_t sync) {
  if (sync)
    hpx_lco_set(sync, 0, NULL, HPX_NULL, HPX_NULL);
}


static size_t _pgas_local_size(gas_t *gas) {
  return global_heap->nbytes;
}


static void *_pgas_local_base(gas_t *gas) {
  return global_heap->base;
}


static gas_t _pgas_vtable = {
  .type          = HPX_GAS_PGAS,
  .delete        = _pgas_delete,
  .join          = pgas_join,
  .leave         = pgas_leave,
  .is_global     = _pgas_is_global,
  .local_size    = _pgas_local_size,
  .local_base    = _pgas_local_base,
  .locality_of   = pgas_gpa_to_rank,
  .sub           = _pgas_sub,
  .add           = _pgas_add,
  .lva_to_gva    = pgas_lva_to_gpa,
  .gva_to_lva    = pgas_gpa_to_lva,
  .there         = _pgas_there,
  .try_pin       = _pgas_try_pin,
  .unpin         = _pgas_unpin,
  .cyclic_alloc  = _pgas_gas_cyclic_alloc,
  .cyclic_calloc = _pgas_gas_cyclic_calloc,
  .local_alloc   = _pgas_gas_alloc,
  .free          = _pgas_gas_free,
  .move          = _pgas_move,
  .memget        = _pgas_parcel_memget,
  .memput        = _pgas_parcel_memput,
  .memcpy        = _pgas_parcel_memcpy,
  .owner_of      = pgas_gpa_to_rank
};

gas_t *gas_pgas_new(size_t heap_size, boot_t *boot, struct transport *transport)
{
  if (here->ranks == 1) {
    dbg_log_gas("PGAS requires at least two ranks\n");
    return NULL;
  }

  if (global_heap) {
    return &_pgas_vtable;
  }

  global_heap = malloc(sizeof(*global_heap));
  if (!global_heap) {
    dbg_error("could not allocate global heap\n");
    goto unwind0;
  }

  int e = heap_init(global_heap, heap_size, (here->rank == 0));
  if (e != LIBHPX_OK) {
    dbg_error("failed to allocate global heap\n");
    goto unwind1;
  }

  if (heap_bind_transport(global_heap, transport) != LIBHPX_OK) {
    dbg_error("failed to bind the transport\n");
  }

  if (mallctl_disable_dirty_page_purge() != LIBHPX_OK) {
    dbg_error("failed to disable dirty page purging\n");
    goto unwind1;
  }

  return &_pgas_vtable;

 unwind1:
  free(global_heap);
 unwind0:
  return NULL;
}
