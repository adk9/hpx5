// ==================================================================-*- C++ -*-
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2016, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================

#ifndef LIBHPX_GAS_AGAS_H
#define LIBHPX_GAS_AGAS_H

#include "libhpx/GAS.h"
#include "libhpx/boot.h"
#include "gva.h"

namespace libhpx {
namespace gas {
class AGAS final : public GAS {
 public:
  AGAS(const config_t* config, boot_t* boot);
  ~AGAS();

  libhpx_gas_t type() const;
  uint64_t maxBlockSize() const;

  void* pinHeap(MemoryOps& memOps, void* key);
  void unpinHeap(MemoryOps&);

  hpx_gas_ptrdiff_t sub(hpx_addr_t lhs, hpx_addr_t rhs, size_t bsize) const;
  hpx_addr_t add(hpx_addr_t gva, hpx_gas_ptrdiff_t n, size_t bsize) const;

  hpx_addr_t there(uint32_t i) const;
  uint32_t ownerOf(hpx_addr_t gva) const;
  bool tryPin(hpx_addr_t gva, void **local);
  void unpin(hpx_addr_t gva);
  void setAttribute(hpx_addr_t gva, uint32_t attr);
  void move(hpx_addr_t src, hpx_addr_t dst, hpx_addr_t lco);

  void free(hpx_addr_t gca, hpx_addr_t rsync);
  hpx_addr_t alloc_cyclic(size_t n, size_t bsize,
                          uint32_t boundary, uint32_t attr);
  hpx_addr_t calloc_cyclic(size_t n, size_t bsize,
                           uint32_t boundary, uint32_t attr);
  hpx_addr_t alloc_blocked(size_t n, size_t bsize,
                           uint32_t boundary, uint32_t attr);
  hpx_addr_t calloc_blocked(size_t n, size_t bsize,
                            uint32_t boundary, uint32_t attr);
  hpx_addr_t alloc_local(size_t n, size_t bsize, uint32_t boundary,
                         uint32_t attr);
  hpx_addr_t calloc_local(size_t n, size_t bsize, uint32_t boundary,
                          uint32_t attr);
  hpx_addr_t alloc_user(size_t n, size_t bsize, uint32_t boundary,
                        hpx_gas_dist_t dist, uint32_t attr);
  hpx_addr_t calloc_user(size_t n, size_t bsize, uint32_t boundary,
                         hpx_gas_dist_t dist, uint32_t attr);

  static void InitGlobalAllocator(GAS* agas);
  static void InitCyclicAllocator(GAS* agas);
  static void* AllocChunk(GAS* agas, void *bitmap, void *addr, size_t n,
                       size_t align);
  static void DeallocChunk(GAS* agas, void *bitmap, void *addr, size_t n);

 private:

  gva_t toGVA(void* lva, size_t bsize);
  hpx_gas_ptrdiff_t subLocal(hpx_addr_t lhs, hpx_addr_t rhs, size_t bsize) const;
  hpx_addr_t addLocal(hpx_addr_t gva, hpx_gas_ptrdiff_t n, size_t bsize) const;

  size_t chunk_size;
  void *chunk_table;
  void *btt;
  void *bitmap;
  void *cyclic_bitmap;
};
}
}

// set the block size of an allocation out-of-band using this TLS
// variable
extern __thread size_t agas_alloc_bsize;

#endif
