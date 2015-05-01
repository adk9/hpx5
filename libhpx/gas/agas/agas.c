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
#include <libhpx/gas.h>
#include <libhpx/locality.h>
#include <libhpx/memory.h>
#include <libhpx/system.h>
#include "agas.h"
#include "btt.h"
#include "chunk_table.h"
#include "gva.h"
#include "../mallctl.h"

static const unsigned AGAS_MAX_RANKS = (1u << GVA_RANK_BITS);
static const uint64_t AGAS_THERE_OFFSET = UINT64_MAX;

static uint64_t size_classes[8] = {
  0,
  8,
  64,
  128,
  1024,
  16384,
  65536,
  1048576
};

typedef struct {
  gas_t vtable;
  size_t chunk_size;
  void *chunk_table;
  void *btt;
  void *bitmap;
} agas_t;

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
  free(agas);
}

static int64_t
_agas_sub(const void *gas, hpx_addr_t lhs, hpx_addr_t rhs, uint32_t bsize) {
  return INT64_MAX;;
}

static hpx_addr_t
_agas_add(const void *gas, hpx_addr_t gva, int64_t bytes, uint32_t bsize) {
  return HPX_NULL;
}

static hpx_addr_t
_agas_there(void *gas, uint32_t i) {
  // We reserve a small range of addresses in the "large" allocation space that
  // will represent locality addresses.
  dbg_assert(i < AGAS_MAX_RANKS);
  dbg_assert(i < here->ranks);

  static const int class = 1;
  uint64_t mask = ~(size_classes[class] - 1);
  gva_t gva = {
    .bits = {
      .offset = AGAS_THERE_OFFSET & mask,
      .home = i,
      .large = 0,
      .size_class = class
    }
  };
  return gva.addr;
}

static bool
_agas_try_pin(void *gas, hpx_addr_t gva, void **lva) {
  agas_t *agas = gas;
  return btt_try_pin(agas->btt, gva, lva);
}

static void
_agas_unpin(void *gas, hpx_addr_t gva) {
  agas_t *agas = gas;
  btt_unpin(agas->btt, gva);
}

static uint32_t
_agas_owner_of(const void *gas, hpx_addr_t gva) {
  const agas_t *agas = gas;
  return btt_owner_of(agas->btt, gva);
}

static void*
_lva_to_chunk(agas_t *gas, void *lva) {
  uintptr_t mask = ~(gas->chunk_size - 1);
  return (void*)((uintptr_t)lva & mask);
}

static hpx_addr_t
_register(agas_t *gas, void *lva) {
  // we need to reverse map this address to an offset into the local portion of
  // the global address space
  void *chunk = _lva_to_chunk(gas, lva);
  uint64_t base = chunk_table_lookup(gas->chunk_table, chunk);
  uint64_t offset = base + ((char*)lva - (char*)chunk);

  // and construct a gva for this
  gva_t gva = {
    .bits = {
      .offset = offset,
      .home = here->rank,
      .large = 0,
      .size_class = 0
    }
  };

  // and insert an entry into our block translation table
  btt_insert(gas->btt, gva.addr, here->rank, lva);

  // and return the address
  return gva.addr;
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

  return _register(gas, lva);
}

static hpx_addr_t
_agas_calloc_local(void *gas, size_t nmemb, size_t size, uint32_t boundary) {
  size_t bytes = nmemb * size;
  void *lva;
  if (boundary) {
    lva = global_memalign(boundary, bytes);
    lva = memset(lva, 0, bytes);
  } else {
    lva = global_calloc(nmemb, size);
  }

  return _register(gas, lva);
}

static void *
_agas_mmap(void *gas, void *addr, size_t n, size_t align) {
  int e;
  agas_t *agas = gas;

  // 1) get gva placement for this allocation
  uint32_t nbits = ceil_div_64(n, agas->chunk_size);
  uint32_t bit;
  e = bitmap_reserve(agas->bitmap, nbits, align, &bit);
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
_agas_munmap(void *gas, void *addr, size_t n) {
  agas_t *agas = gas;

  // 1) release the bits
  uint64_t offset = chunk_table_lookup(agas->chunk_table, addr);
  uint32_t nbits = ceil_div_64(n, agas->chunk_size);
  bitmap_release(agas->bitmap, offset, nbits);

  // 2) unmap the backing memory
  system_munmap(NULL, addr, n);

  // 3) remove the inverse mappings
  char *chunk = addr;
  for (int i = 0; i < nbits; ++i) {
    chunk_table_remove(agas->chunk_table, chunk);
    chunk += agas->chunk_size;
  }
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
  .alloc_cyclic   = NULL,
  .calloc_cyclic  = NULL,
  .alloc_blocked  = NULL,
  .calloc_blocked = NULL,
  .alloc_local    = _agas_alloc_local,
  .calloc_local   = _agas_calloc_local,
  .free           = NULL,
  .move           = NULL,
  .memget         = NULL,
  .memput         = NULL,
  .memcpy         = NULL,
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
  btt_insert(agas->btt, _agas_there(agas, here->rank), here->rank, here);
  return &agas->vtable;
}
