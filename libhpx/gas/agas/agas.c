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

#include <stdlib.h>
#include <libhpx/bitmap.h>
#include <libhpx/boot.h>
#include <libhpx/debug.h>
#include <libhpx/gpa.h>
#include <libhpx/locality.h>
#include <libhpx/memory.h>
#include <libhpx/system.h>
#include "agas.h"
#include "btt.h"
#include "chunk_table.h"
#include "gva.h"
#include "../mallctl.h"

static const uint64_t AGAS_THERE_OFFSET = UINT64_MAX;

HPX_ACTION_DECL(agas_alloc_cyclic);
HPX_ACTION_DECL(agas_calloc_cyclic);

static void
_agas_delete(void *gas) {
  agas_t *agas = gas;
  if (agas->chunk_table) {
    chunk_table_delete(agas->chunk_table);
  }
  if (agas->btt) {
    btt_delete(agas->btt);
  }
  if (agas->bitmap) {
    bitmap_delete(agas->bitmap);
  }
  if (agas->cyclic_bitmap) {
    bitmap_delete(agas->cyclic_bitmap);
  }
  free(agas);
}

static int64_t
_agas_sub(const void *gas, hpx_addr_t lhs, hpx_addr_t rhs, uint32_t bsize) {
  gva_t l = { .addr = lhs };
  gva_t r = { .addr = rhs };

  uint32_t size = ceil_log2_32(bsize);
  if (l.bits.size != size || r.bits.size != size) {
    dbg_error("block size does not match\n");
  }

  if (l.bits.cyclic && r.bits.cyclic) {
    return gpa_sub_cyclic(lhs, rhs, bsize);
  }

  if (!l.bits.cyclic && !r.bits.cyclic) {
    if (l.bits.home == r.bits.home) {
      return agas_local_sub(gas, l, r, bsize);
    }
  }

  dbg_error("could not compare pointers between allocations\n");
}

static hpx_addr_t
_agas_add(const void *gas, hpx_addr_t addr, int64_t bytes, uint32_t bsize) {
  gva_t gva = { .addr = addr };
  uint32_t size = ceil_log2_32(bsize);
  if (gva.bits.size != size) {
    dbg_error("block size does not match\n");
  }

  if (gva.bits.cyclic) {
    gva.addr = gpa_add_cyclic(addr, bytes, bsize);
    gva.bits.size = size;
    gva.bits.cyclic = 1;
    return gva.addr;
  }
  else {
    return agas_local_add(gas, gva, bytes, bsize);
  }
}

static hpx_addr_t
_agas_there(void *gas, uint32_t i) {
  // We reserve a small range of addresses in the "large" allocation space that
  // will represent locality addresses.
  dbg_assert(i < here->ranks);
  gva_t gva = {
    .bits = {
      .offset = AGAS_THERE_OFFSET,
      .size   = 0,
      .cyclic  = 0,
      .home   = (uint16_t)i,
    }
  };
  return gva.addr;
}

static bool
_agas_try_pin(void *gas, hpx_addr_t addr, void **lva) {
  agas_t *agas = gas;
  gva_t gva = { .addr = addr };
  return btt_try_pin(agas->btt, gva, lva);
}

static void
_agas_unpin(void *gas, hpx_addr_t addr) {
  agas_t *agas = gas;
  gva_t gva = { .addr = addr };
  btt_unpin(agas->btt, gva);
}

static uint32_t
_agas_owner_of(const void *gas, hpx_addr_t addr) {
  const agas_t *agas = gas;
  gva_t gva = { .addr = addr };
  return btt_owner_of(agas->btt, gva);
}

static void*
_lva_to_chunk(agas_t *gas, void *lva) {
  uintptr_t mask = ~(gas->chunk_size - 1);
  return (void*)((uintptr_t)lva & mask);
}

static gva_t
_lva_to_gva(agas_t *gas, void *lva, uint32_t bsize) {
  // we need to reverse map this address to an offset into the local portion of
  // the global address space
  void *chunk = _lva_to_chunk(gas, lva);
  uint64_t base = chunk_table_lookup(gas->chunk_table, chunk);
  uint64_t offset = base + ((char*)lva - (char*)chunk);

  // and construct a gva for this
  gva_t gva = {
    .bits = {
      .offset = offset,
      .size   = ceil_log2_32(bsize),
      .cyclic  = 0,
      .home   = here->rank,
    }
  };

  // and return the address
  return gva;
}

static void *
_chunk_alloc(void *bitmap, void *addr, size_t n, size_t align) {
  agas_t *agas = (agas_t*)here->gas;

  // 1) get gva placement for this allocation
  uint32_t nbits = ceil_div_64(n, agas->chunk_size);
  uint32_t log2_align = ceil_log2_size_t(align);
  uint32_t bit;
  int e = bitmap_reserve(bitmap, nbits, log2_align, &bit);
  dbg_check(e, "Could not reserve gva for %lu bytes\n", n);
  uint64_t offset = bit * agas->chunk_size;

  // 2) get backing memory
  void *base = system_mmap(NULL, addr, n, align);
  dbg_assert(base);
  dbg_assert(((uintptr_t)base & (align - 1)) == 0);

  // 3) insert the inverse mappings
  char* chunk = base;
  for (int i = 0; i < nbits; i++) {
    chunk_table_insert(agas->chunk_table, chunk, offset);
    offset += agas->chunk_size;
    chunk += agas->chunk_size;
  }
  return base;
}

static void
_chunk_dalloc(void *bitmap, void *addr, size_t n) {
  agas_t *agas = (agas_t*)here->gas;

  // 1) release the bits
  uint64_t offset = chunk_table_lookup(agas->chunk_table, addr);
  uint32_t nbits = ceil_div_64(n, agas->chunk_size);
  uint32_t bit = offset / agas->chunk_size;
  bitmap_release(bitmap, bit, nbits);

  // 2) unmap the backing memory
  system_munmap(NULL, addr, n);

  // 3) remove the inverse mappings
  char *chunk = addr;
  for (int i = 0; i < nbits; ++i) {
    chunk_table_remove(agas->chunk_table, chunk);
    chunk += agas->chunk_size;
  }
}

static void *_chunk_alloc_cyclic(void *addr, size_t size, size_t align,
                                 bool *zero, unsigned UNUSED) {
  agas_t *agas = (agas_t*)here->gas;
  assert(size % agas->chunk_size == 0);
  assert(size / agas->chunk_size < UINT32_MAX);
  void *chunk = _chunk_alloc(agas->cyclic_bitmap, addr, size, align);

  if (zero && *zero)
    memset(chunk, 0, size);
  return chunk;
}

static bool _chunk_dalloc_cyclic(void *chunk, size_t size, unsigned UNUSED) {
  agas_t *agas = (agas_t*)here->gas;
  _chunk_dalloc(agas->cyclic_bitmap, chunk, size);
  return true;
}

static int
_locality_alloc_cyclic_handler(uint64_t blocks, uint32_t align,
                               uint64_t offset, void *lva, int zero) {
  agas_t *agas = (agas_t*)here->gas;
  uint32_t bsize = 1u << align;
  if (here->rank != 0) {
    posix_memalign(&lva, bsize, blocks * bsize);
  }

  if (zero) {
    lva = memset(lva, 0, blocks * bsize);
  }

  // and insert entries into our block translation table
  for (int i = 0; i < blocks; i++) {
    gva_t gva = {
      .bits = {
        .offset = offset + (i * bsize),
        .cyclic = 1,
        .size = align,
        .home = here->rank
      }
    };
    void *block = lva + (i * bsize);
    btt_insert(agas->btt, gva, here->rank, block, blocks);
  }

  return HPX_SUCCESS;
}
HPX_ACTION(HPX_DEFAULT, 0, _locality_alloc_cyclic,
           _locality_alloc_cyclic_handler, HPX_UINT64,
           HPX_UINT32, HPX_UINT64, HPX_POINTER, HPX_INT);

hpx_addr_t _agas_alloc_cyclic_sync(size_t n, uint32_t bsize, int zero) {
  agas_t *agas = (agas_t*)here->gas;
  dbg_assert(agas->cyclic_arena < UINT32_MAX);
  dbg_assert(here->rank == 0);

  // Figure out how many blocks per node we need.
  uint64_t blocks = ceil_div_64(n, here->ranks);
  uint32_t  align = ceil_log2_32(bsize);
  dbg_assert(align < 32);
  uint32_t padded = 1u << align;
  int flags = MALLOCX_LG_ALIGN(align) | MALLOCX_ARENA(agas->cyclic_arena);
  void *lva = libhpx_global_mallocx(blocks * padded, flags);
  if (!lva) {
    dbg_error("failed cyclic allocation\n");
  }

  gva_t gva = _lva_to_gva(agas, lva, bsize);
  gva.bits.cyclic = 1;
  uint64_t offset = gva.bits.offset;
  int e = hpx_bcast_rsync(_locality_alloc_cyclic, &blocks, &align, &offset, &lva, &zero);
  dbg_check(e, "failed to insert btt entries.\n");

  // and return the address
  return gva.addr;
}

hpx_addr_t agas_alloc_cyclic_sync(size_t n, uint32_t bsize) {
  dbg_assert(here->rank == 0);
  return _agas_alloc_cyclic_sync(n, bsize, 0);
}

static int _alloc_cyclic_handler(size_t n, size_t bsize) {
  hpx_addr_t addr = agas_alloc_cyclic_sync(n, bsize);
  HPX_THREAD_CONTINUE(addr);
}
HPX_ACTION(HPX_DEFAULT, 0, agas_alloc_cyclic, _alloc_cyclic_handler, HPX_SIZE_T,
           HPX_SIZE_T);

static hpx_addr_t
_agas_alloc_cyclic(size_t n, uint32_t bsize, uint32_t boundary) {
  hpx_addr_t addr;
  if (here->rank == 0) {
    addr = agas_alloc_cyclic_sync(n, bsize);
  }
  else {
    int e = hpx_call_sync(HPX_THERE(0), agas_alloc_cyclic, &addr, sizeof(addr),
                          &n, &bsize);
    dbg_check(e, "Failed to call agas_alloc_cyclic_handler.\n");
  }
  dbg_assert_str(addr != HPX_NULL, "HPX_NULL is not a valid allocation\n");
  return addr;
}

void agas_free_cyclic_sync(void *lva) {
  agas_t *agas = (agas_t*)here->gas;
  dbg_assert(agas->cyclic_arena < UINT32_MAX);
  dbg_assert(here->rank == 0);

  int flags = MALLOCX_ARENA(agas->cyclic_arena);
  libhpx_global_dallocx(lva, flags);
}

hpx_addr_t agas_calloc_cyclic_sync(size_t n, uint32_t bsize) {
  assert(here->rank == 0);
  return _agas_alloc_cyclic_sync(n, bsize, 1);
}

static int _calloc_cyclic_handler(size_t n, size_t bsize) {
  hpx_addr_t addr = agas_calloc_cyclic_sync(n, bsize);
  HPX_THREAD_CONTINUE(addr);
}
HPX_ACTION(HPX_DEFAULT, 0, agas_calloc_cyclic, _calloc_cyclic_handler, HPX_SIZE_T,
           HPX_SIZE_T);

static hpx_addr_t
_agas_calloc_cyclic(size_t n, uint32_t bsize, uint32_t boundary) {
  hpx_addr_t addr;
  if (here->rank == 0) {
    addr = agas_calloc_cyclic_sync(n, bsize);
  }
  else {
    int e = hpx_call_sync(HPX_THERE(0), agas_calloc_cyclic, &addr, sizeof(addr),
                          &n, &bsize);
    dbg_check(e, "Failed to call agas_calloc_cyclic_handler.\n");
  }
  dbg_assert_str(addr != HPX_NULL, "HPX_NULL is not a valid allocation\n");
  return addr;
}

static hpx_addr_t
_agas_alloc_local(void *gas, uint32_t bytes, uint32_t boundary) {
  // use the local allocator to get some memory that is part of the global
  // address space
  void *lva = NULL;
  if (boundary) {
    lva = global_memalign(boundary, bytes);
  }
  else {
    lva = global_malloc(bytes);
  }

  agas_t *agas = gas;
  gva_t gva = _lva_to_gva(gas, lva, bytes);
  btt_insert(agas->btt, gva, here->rank, lva, 1);
  return gva.addr;
}

static hpx_addr_t
_agas_calloc_local(void *gas, size_t nmemb, size_t size, uint32_t boundary) {
  dbg_assert(size < UINT32_MAX);

  size_t bytes = nmemb * size;
  char *lva;
  if (boundary) {
    lva = global_memalign(boundary, bytes);
    lva = memset(lva, 0, bytes);
  } else {
    lva = global_calloc(nmemb, size);
  }

  agas_t *agas = gas;
  gva_t gva = _lva_to_gva(gas, lva, size);
  hpx_addr_t base = gva.addr;
  uint32_t bsize = 1lu << gva.bits.size;
  for (int i = 0; i < nmemb; i++) {
    btt_insert(agas->btt, gva, here->rank, lva, nmemb);
    lva += bsize;
    gva.bits.offset += bsize;
  }
  return base;
}

static int
_agas_free_cyclic_async_handler(hpx_addr_t base) {
  gva_t gva = (gva_t)base;
  agas_t *agas = (agas_t*)here->gas;
  gva.bits.home = here->rank;
  void *lva = btt_lookup(agas->btt, gva);
  if (!lva) {
    return HPX_SUCCESS;
  }

  // remove all btt entries
  uint64_t offset = gva.bits.offset;
  size_t blocks = btt_get_blocks(agas->btt, gva);
  size_t bsize = 1 << gva.bits.size;
  for (int i = 0; i < blocks; i++) {
    gva.bits.offset = offset + (i * bsize);
    gva.bits.home = here->rank;
    btt_remove(agas->btt, gva);
  }

  // and free the backing memory
  if (here->rank == 0) {
    agas_free_cyclic_sync(lva);
  } else {
    free(lva);
  }
  return HPX_SUCCESS;
}
HPX_ACTION(HPX_DEFAULT, 0, _agas_free_cyclic_async,
           _agas_free_cyclic_async_handler, HPX_ADDR);

static int
_agas_free_local_async_handler(hpx_addr_t rsync) {
  hpx_addr_t gva = hpx_thread_current_target();
  hpx_gas_free(gva, rsync);
  return HPX_SUCCESS;
}
HPX_ACTION(HPX_DEFAULT, 0, _agas_free_local_async,
           _agas_free_local_async_handler, HPX_ADDR);

static void
_agas_free(void *gas, hpx_addr_t addr, hpx_addr_t rsync) {
  if (addr == HPX_NULL) {
    return;
  }

  gva_t gva = { .addr = addr };

  if (gva.bits.cyclic) {
    int e = hpx_bcast_lsync(_agas_free_cyclic_async, rsync, &addr);
    dbg_check(e, "failed to broadcast AGAS cyclic free operation\n");
    return;
  }

  agas_t *agas = gas;
  void *lva = btt_lookup(agas->btt, gva);
  if (lva) {
    agas_local_free(agas, gva, lva, rsync);
    return;
  }

  int e = hpx_xcall(addr, _agas_free_local_async, HPX_NULL, rsync);
  dbg_check(e, "failed to forward AGAS free operation\n");
  (void)e;
}

static void *
_agas_mmap(void *gas, void *addr, size_t n, size_t align) {
  agas_t *agas = gas;
  return _chunk_alloc(agas->bitmap, addr, n, align);
}

static void
_agas_munmap(void *gas, void *addr, size_t n) {
  agas_t *agas = gas;
  _chunk_dalloc(agas->bitmap, addr, n);
}

static gas_t _agas_vtable = {
  .type           = HPX_GAS_AGAS,
  .delete         = _agas_delete,
  .local_size     = NULL,
  .local_base     = NULL,
  .sub            = _agas_sub,
  .add            = _agas_add,
  .there          = _agas_there,
  .try_pin        = _agas_try_pin,
  .unpin          = _agas_unpin,
  .alloc_cyclic   = _agas_alloc_cyclic,
  .calloc_cyclic  = _agas_calloc_cyclic,
  .alloc_blocked  = NULL,
  .calloc_blocked = NULL,
  .alloc_local    = _agas_alloc_local,
  .calloc_local   = _agas_calloc_local,
  .free           = _agas_free,
  .move           = agas_move,
  .memget         = agas_memget,
  .memput         = agas_memput,
  .memcpy         = agas_memcpy,
  .owner_of       = _agas_owner_of,
  .mmap           = _agas_mmap,
  .munmap         = _agas_munmap
};

gas_t *gas_agas_new(const config_t *config, boot_t *boot) {
  agas_t *agas = malloc(sizeof(*agas));
  agas->vtable = _agas_vtable;
  agas->chunk_table = chunk_table_new(0);
  agas->btt = btt_new(0);

  agas->chunk_size = mallctl_get_chunk_size();
  size_t heap_size = 1lu << GVA_OFFSET_BITS;
  size_t nchunks = ceil_div_64(heap_size, agas->chunk_size);
  uint32_t min_align = ceil_log2_64(agas->chunk_size);
  uint32_t base_align = ceil_log2_64(heap_size);
  agas->bitmap = bitmap_new(nchunks, min_align, base_align);

  if (here->rank == 0) {
    size_t nchunks = ceil_div_64(here->ranks * heap_size, agas->chunk_size);
    uint32_t min_align = ceil_log2_64(agas->chunk_size);
    uint32_t base_align = ceil_log2_64(heap_size);
    agas->cyclic_bitmap = bitmap_new(nchunks, min_align, base_align);

    agas->cyclic_arena = mallctl_create_arena(_chunk_alloc_cyclic,
                                              _chunk_dalloc_cyclic);
    log_gas("allocated the arena to manage cyclic allocations.\n");
  }

  gva_t there = { .addr = _agas_there(agas, here->rank) };
  btt_insert(agas->btt, there, here->rank, here, 1);
  return &agas->vtable;
}
