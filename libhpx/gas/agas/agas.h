// =============================================================================
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

#include <hpx/attributes.h>
#include <libhpx/gas.h>
#include "gva.h"

#ifdef __cplusplus
extern "C" {
#endif

struct boot;
struct config;

typedef struct {
  gas_t vtable;
  size_t chunk_size;
  void *chunk_table;
  void *btt;
  void *bitmap;
  void *cyclic_bitmap;
} agas_t;

// set the block size of an allocation out-of-band using this TLS
// variable
extern __thread size_t agas_alloc_bsize;

struct gas *gas_agas_new(const struct config *config, struct boot *boot);

void agas_global_allocator_init(agas_t *agas);
void agas_cyclic_allocator_init(agas_t *agas);

void *agas_chunk_alloc(agas_t *agas, void *bitmap, void *addr, size_t n,
                       size_t align);
void agas_chunk_dalloc(agas_t *agas, void *bitmap, void *addr, size_t n);

void agas_move(void *gas, hpx_addr_t src, hpx_addr_t dst, hpx_addr_t sync);

gva_t agas_lva_to_gva(agas_t *gas, void *lva, size_t bsize);

void agas_free(void *gas, hpx_addr_t addr, hpx_addr_t rsync);

/// Local allocation operations.
hpx_addr_t agas_alloc_local(size_t n, size_t bsize, uint32_t boundary,
                            uint32_t attr);
hpx_addr_t agas_calloc_local(size_t n, size_t bsize, uint32_t boundary,
                             uint32_t attr);
int64_t agas_sub_local(const agas_t *agas, gva_t lhs, gva_t rhs, size_t bsize);
hpx_addr_t agas_add_local(const agas_t *agas, gva_t gva, int64_t bytes,
                          size_t bsize);

/// Cyclic allocation operations.
hpx_addr_t agas_alloc_cyclic(size_t n, size_t bbsize, uint32_t boundary,
                             uint32_t attr);
hpx_addr_t agas_calloc_cyclic(size_t n, size_t bbsize, uint32_t boundary,
                              uint32_t attr);

/// User-defined allocation operations.
hpx_addr_t agas_alloc_user(size_t n, size_t bbsize, uint32_t boundary,
                           hpx_gas_dist_t dist, uint32_t attr);
hpx_addr_t agas_calloc_user(size_t n, size_t bbsize, uint32_t boundary,
                            hpx_gas_dist_t dist, uint32_t attr);

/// String operations.
int agas_memput(void *gas, hpx_addr_t to, const void *from, size_t n,
                hpx_addr_t lsync, hpx_addr_t rsync);
int agas_memput_lsync(void *gas, hpx_addr_t to, const void *from, size_t n,
                      hpx_addr_t rsync);
int agas_memput_rsync(void *gas, hpx_addr_t to, const void *from, size_t n);
int agas_memget(void *gas, void *to, hpx_addr_t from, size_t n,
                hpx_addr_t lsync, hpx_addr_t rsync);
int agas_memget_rsync(void *gas, void *to, hpx_addr_t from, size_t n,
                      hpx_addr_t lsync);
int agas_memget_lsync(void *gas, void *to, hpx_addr_t from, size_t n);
int agas_memcpy(void *gas, hpx_addr_t to, hpx_addr_t from, size_t size,
                hpx_addr_t sync);
int agas_memcpy_sync(void *gas, hpx_addr_t to, hpx_addr_t from, size_t size);


#ifdef __cplusplus
}
#endif

#endif
